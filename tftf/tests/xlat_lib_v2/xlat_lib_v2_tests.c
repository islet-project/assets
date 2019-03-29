/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <debug.h>
#include <errno.h>
#include <platform_def.h>
#include <stdlib.h>
#include <string.h>
#include <tftf_lib.h>
#include <xlat_tables_v2.h>

/*
 * NOTE: In order to make the tests as generic as possible, the tests don't
 * actually access the mapped memory, the instruction AT is used to verify that
 * the mapping is correct. It is likely that the memory that ends up being
 * mapped isn't physically there, so the memory is mapped as device memory to
 * prevent the CPU from speculatively reading from it.
 *
 * Also, it is very likely that a failure in any of the tests would leave the
 * translation tables in a state from which the system can't be recovered. This
 * is why in some cases the tests don't try to unmap regions that have been
 * successfully mapped.
 */

#define SIZE_L1		XLAT_BLOCK_SIZE(1)
#define SIZE_L2		XLAT_BLOCK_SIZE(2)
#define SIZE_L3		XLAT_BLOCK_SIZE(3) /* PAGE_SIZE */

#define MASK_L1		XLAT_BLOCK_MASK(1)
#define MASK_L2		XLAT_BLOCK_MASK(2)
#define MASK_L3		XLAT_BLOCK_MASK(3)

/*
 * Translate the given virtual address into a physical address in the current
 * translation regime. Returns the resulting physical address on success,
 * otherwise UINT64_MAX.
 */
static unsigned long long va2pa(uintptr_t base_va)
{
	uint64_t par;

	/*
	 * Performs stage 1 address translation for the current EL, with
	 * read permissions.
	 */
#ifdef AARCH32
	if (IS_IN_HYP())
		write_ats1hr(base_va);
	else
		write_ats1cpr(base_va);
	isb();
	par = read64_par();
#else
	if (IS_IN_EL2())
		ats1e2r(base_va);
	else
		ats1e1r(base_va);
	isb();
	par = read_par_el1();
#endif

	/*
	 * If PAR_EL1.F == 1 then the address translation was aborted.
	 * In this case, return an invalid VA.
	 */
	if (par & PAR_F_MASK)
		return UINT64_MAX;

	/*
	 * If PAR_EL1.F == 0 then the address translation completed
	 * successfully. In this case, bits 47-12 hold bits 47-12 of the
	 * resulting physical address.
	 */
	return par & (PAR_ADDR_MASK << PAR_ADDR_SHIFT);
}

/*
 * Checks that the given region has been mapped correctly. Returns 0 on success,
 * 1 otherwise.
 */
static int verify_region_mapped(unsigned long long base_pa, uintptr_t base_va,
				size_t size)
{
	uintptr_t end_va = base_va + size;
	unsigned long long addr;

	VERBOSE("Checking: PA = 0x%llx, VA = 0x%lx, size = 0x%zx\n",
		base_pa, base_va, size);

	while (base_va < end_va) {
		addr = va2pa(base_va);

		if (base_pa != addr) {
			ERROR("Error: 0x%lx => 0x%llx (expected 0x%llx)\n",
			      base_va, addr, base_pa);
			return 1;
		}

		base_va += PAGE_SIZE;
		base_pa += PAGE_SIZE;
	}

	return 0;
}

/*
 * Checks that the given region has been unmapped correctly. Returns 0 on
 * success, 1 otherwise.
 */
static int verify_region_unmapped(uintptr_t base_va, size_t size)
{
	uintptr_t end_va = base_va + size;

	VERBOSE("Checking: VA = 0x%lx, size = 0x%zx\n", base_va, size);

	while (base_va < end_va) {
		unsigned long long phys_addr = va2pa(base_va);

		if (phys_addr != UINT64_MAX) {
			ERROR("Error: 0x%lx => 0x%llx (expected UINT64_MAX)\n",
			      base_va, phys_addr);
			return 1;
		}

		base_va += PAGE_SIZE;
	}

	return 0;
}

/*
 * Ask to map a given region of physical memory with a given set of memory
 * attributes. Returns 0 on success, otherwise an error code returned by
 * mmap_add_dynamic_region(). On success, it also verifies that the mapping has
 * been done correctly.
 */
static int add_region(unsigned long long base_pa, uintptr_t base_va,
		      size_t size, unsigned int attr)
{
	int ret;

	VERBOSE("mmap_add_dynamic_region(0x%llx, 0x%lx, 0x%zx, 0x%x)\n",
		base_pa, base_va, size, attr);

	ret = mmap_add_dynamic_region(base_pa, base_va, size, attr);

	VERBOSE(" = %d\n", ret);

	if (ret == 0) {
		return verify_region_mapped(base_pa, base_va, size);
	}

	return ret;
}

/*
 * Ask to map a given region of physical memory with a given set of memory
 * attributes. On success it returns a VA with the VA allocated for the new
 * region and 0 as error code. Otherwise, the error code returned by
 * mmap_add_dynamic_region_alloc_va().
 */
static int add_region_alloc_va(unsigned long long base_pa, uintptr_t *base_va,
			       size_t size, unsigned int attr)
{
	int ret;

	VERBOSE("mmap_add_dynamic_region_alloc_va(0x%llx, 0x%zx, 0x%x)\n",
		base_pa, size, attr);

	ret = mmap_add_dynamic_region_alloc_va(base_pa, base_va, size, attr);

	VERBOSE(" = %d VA=0x%lx\n", ret, *base_va);

	if (ret == 0) {
		return verify_region_mapped(base_pa, *base_va, size);
	}

	return ret;
}

/*
 * Unmap a given memory region given its virtual address and size.
 */
static int remove_region(uintptr_t base_va, size_t size)
{
	int ret;

	VERBOSE("mmap_remove_dynamic_region(0x%lx, 0x%zx)\n", base_va, size);

	ret = mmap_remove_dynamic_region(base_va, size);

	VERBOSE(" = %d\n", ret);

	if (ret == 0) {
		return verify_region_unmapped(base_va, size);
	}

	return ret;
}

/**
 * @Test_Aim@ Perform dynamic translation tables API basic tests
 *
 * This test checks for invalid uses of the dynamic translation tables library.
 */
test_result_t xlat_lib_v2_basic_test(void)
{
	uintptr_t memory_base_va;
	int rc, i;

	/*
	 * 1) Try to allocate a region with size 0.
	 *
	 * The allocation should "succeed" but not allocate anything, and it
	 * still should return the top VA.
	 */
	rc = add_region_alloc_va(0, &memory_base_va, 0, 0);
	if (rc != 0) {
		tftf_testcase_printf("%d: add_region_alloc_va: %d\n",
				     __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	/*
	 * Try do deallocate this region. It should fail because it hasn't been
	 * allocated in the first place.
	 */
	rc = remove_region(memory_base_va, 0);
	if (rc != -EINVAL) {
		if (rc == 0) {
			tftf_testcase_printf("%d: Deallocation should have failed.\n",
					     __LINE__);
		} else {
			tftf_testcase_printf("%d: remove_region: %d\n",
					     __LINE__, rc);
		}
		return TEST_RESULT_FAIL;
	}

	/* 2) Allocate and deallocate a small region */

	rc = add_region_alloc_va(0, &memory_base_va, SIZE_L3, MT_DEVICE);
	if (rc != 0) {
		tftf_testcase_printf("%d: add_region_alloc_va: %d\n",
				     __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	rc = remove_region(memory_base_va, SIZE_L3);
	if (rc != 0) {
		tftf_testcase_printf("%d: remove_region: %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	/*
	 * 3) Try to allocate the last page of the virtual address space, which
	 * can lead to wraparound problems (specially in AArch32).
	 */
	rc = add_region(PLAT_VIRT_ADDR_SPACE_SIZE - PAGE_SIZE,
			PLAT_VIRT_ADDR_SPACE_SIZE - PAGE_SIZE,
			PAGE_SIZE, MT_DEVICE);
	if (rc != 0) {
		tftf_testcase_printf("%d: add_region: %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	rc = remove_region(PLAT_VIRT_ADDR_SPACE_SIZE - PAGE_SIZE, PAGE_SIZE);
	if (rc != 0) {
		tftf_testcase_printf("%d: remove_region: %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	/*
	 * 4) Try to allocate an invalid region. It should fail, but it will
	 * return the address of memory that can be used for the following
	 * tests.
	 */
	rc = add_region_alloc_va(0, &memory_base_va, SIZE_MAX, MT_DEVICE);
	if (rc == 0) {
		tftf_testcase_printf("%d: add_region_alloc_va() didn't fail\n",
				     __LINE__);

		return TEST_RESULT_FAIL;
	}

#if AARCH64
	unsigned long long memory_base_pa;

	/*
	 * Get address of memory region over the max used VA that is aligned to
	 * a L1 block for the next tests.
	 */
	memory_base_pa = (memory_base_va + SIZE_L1 - 1ULL) & ~MASK_L1;

	INFO("Using 0x%llx as base address for tests.\n", memory_base_pa);

	/*
	 * 5) Try to allocate memory over the virtual address space limit. This
	 * test can't run in AArch32 because size_t is 32-bit wide, and the
	 * address space used by the TFTF is also 32-bit wide, so it is not
	 * possible to go over the limit.
	 */

	rc = add_region(memory_base_pa, memory_base_va,
			PLAT_VIRT_ADDR_SPACE_SIZE + PAGE_SIZE - memory_base_pa,
			MT_DEVICE);
	if (rc != -ERANGE) {
		tftf_testcase_printf("%d: Allocation succeeded: %d\n",
				     __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	/* Try to wrap around 64 bit */
	rc = add_region(1ULL << 32, 1ULL << 32,
			UINT64_MAX - PAGE_SIZE + 1ULL,
			MT_DEVICE);
	if (rc != -ERANGE) {
		tftf_testcase_printf("%d: Allocation succeeded: %d\n",
				     __LINE__, rc);
		return TEST_RESULT_FAIL;
	}
#else
	/* Try to wrap around 32 bit */
	rc = add_region((1ULL << 32) - PAGE_SIZE, (1ULL << 32) - PAGE_SIZE,
			2 * PAGE_SIZE, MT_DEVICE);
	if (rc != -ERANGE) {
		tftf_testcase_printf("%d: Allocation succeeded: %d\n",
				     __LINE__, rc);
		return TEST_RESULT_FAIL;
	}
#endif

	/*
	 * 6) Try to allocate too many regions. There is only room for at most
	 * MAX_MMAP_REGIONS, and some of the regions are already used for
	 * devices, code, BSS, etc. Trying to allocate MAX_MMAP_REGIONS here
	 * should fail.
	 */
	for (i = 0; i < MAX_MMAP_REGIONS; i++) {
		uintptr_t addr = memory_base_va + PAGE_SIZE * i;

		rc = add_region(addr, addr, PAGE_SIZE, MT_DEVICE);
		if (rc == -ENOMEM) {
			/* The limit has been reached as expected */
			break;
		} else if (rc != 0) {
			tftf_testcase_printf("%d: add_region: %d\n",
					     __LINE__, rc);
			return TEST_RESULT_FAIL;
		}
	}

	if (i == MAX_MMAP_REGIONS) {
		tftf_testcase_printf("%d: Too many regions allocated\n",
				     __LINE__);
		return TEST_RESULT_FAIL;
	}

	i--;

	/* Cleanup */
	for (; i >= 0; i--) {
		uintptr_t addr = memory_base_va + PAGE_SIZE * i;

		rc = remove_region(addr,  PAGE_SIZE);
		if (rc != 0) {
			tftf_testcase_printf("%d: remove_region: %d\n",
					     __LINE__, rc);
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}
