/*
 * Copyright (c) 2019-2020, Arm Limited. All rights reserved.
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

#define STRESS_TEST_ITERATIONS		1000

#define SIZE_L1		XLAT_BLOCK_SIZE(1)
#define SIZE_L2		XLAT_BLOCK_SIZE(2)
#define SIZE_L3		XLAT_BLOCK_SIZE(3) /* PAGE_SIZE */

#define MASK_L1		XLAT_BLOCK_MASK(1)
#define MASK_L2		XLAT_BLOCK_MASK(2)
#define MASK_L3		XLAT_BLOCK_MASK(3)

static const struct {
	size_t size;
	size_t expected_va_mask;
} mem_tests[] = {
	{ SIZE_L1 + 2 * SIZE_L2 + 2 * SIZE_L3,	MASK_L3 },
	{ SIZE_L1 + SIZE_L2 + SIZE_L3,		MASK_L3 },
	{ SIZE_L1 + 2 * SIZE_L2,		MASK_L2 },
	{ SIZE_L1 + SIZE_L2,			MASK_L2 },
	{ SIZE_L1 + 2 * SIZE_L3,		MASK_L3 },
	{ SIZE_L1 + SIZE_L3,			MASK_L3 },
	{ SIZE_L1,				MASK_L1 },
	{ SIZE_L2 + 2 * SIZE_L3,		MASK_L3 },
	{ SIZE_L2 + SIZE_L3,			MASK_L3 },
	{ SIZE_L2,				MASK_L2 },
	{ SIZE_L3,				MASK_L3 }
};

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
#ifndef __aarch64__
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

/*
 * Number of individual chunks of memory that can be mapped and unmaped in the
 * region that we use for testing. The size of each block is total_size /
 * num_blocks. The test tries to allocate as much memory as possible.
 */
#define STRESS_TEST_NUM_BLOCKS		1024

/* Memory region to be used by the stress test */
static uintptr_t memory_base_va;
static size_t memory_size;
/* Block size to be used by the stress test */
static size_t block_size;
/*
 * Each element of the array can have one of the following states:
 * - 0: Free
 * - 1: Used
 * - 2: Used, and it is the start of a region
 */
static int block_used[STRESS_TEST_NUM_BLOCKS];

/* Returns -1 if error, 1 if chunk added, 0 if not added */
static int alloc_random_chunk(void)
{
	int rc;
	int start = rand() % STRESS_TEST_NUM_BLOCKS;
	int blocks = rand() % STRESS_TEST_NUM_BLOCKS;
	bool is_free = true;

	if (start + blocks > STRESS_TEST_NUM_BLOCKS) {
		blocks = STRESS_TEST_NUM_BLOCKS - start;
	}

	/* Check if it's free */
	for (int i = start; i < start + blocks; i++) {
		if (block_used[i] != 0) {
			is_free = false;
			break;
		}
	}

	uintptr_t base_va = memory_base_va + start * block_size;
	unsigned long long base_pa = base_va;
	size_t size = blocks * block_size;

	if (is_free) {
		/*
		 * Allocate a region, it should succeed. Use a non 1:1 mapping
		 * by adding an arbitrary offset to the base PA.
		 */
		rc = add_region(base_pa + 0x20000U, base_va, size, MT_DEVICE);
		if ((rc == -ENOMEM) || (rc == -EPERM)) {
			/*
			 * Not enough memory or partial overlap, don't consider
			 * this a hard failure.
			 */
			return 0;
		} else if (rc != 0) {
			tftf_testcase_printf("%d: add_region failed: %d\n",
					     __LINE__, rc);
			return -1;
		}

		/* Flag as used */
		block_used[start] = 2;
		for (int i = start + 1; i < start + blocks; i++)
			block_used[i] = 1;

		return 1;
	} else {
		/* Allocate, it should fail */
		rc = add_region(base_pa, base_va, size, MT_DEVICE);
		if (rc == 0) {
			tftf_testcase_printf("%d: add_region succeeded\n",
					     __LINE__);
			return -1;
		}

		return 0;
	}
}

/* Returns -1 if error, 1 if chunk removed, 0 if not removed */
static int free_random_chunk(void)
{
	int start = -1;
	int end = -1;
	int seek = rand() % STRESS_TEST_NUM_BLOCKS;
	int i = seek;

	for (;;) {
		if (start == -1) { /* Look for the start of a block */

			/* Search the start of a chunk */
			if (block_used[i] == 2) {
				start = i;
			}

		} else { /* Look for the end of the block */

			/* Search free space or the start of another chunk */
			if (block_used[i] != 1) {
				end = i;
				break;
			}
		}

		i++;

		if (start == -1) { /* Look for the start of a block */

			/* Looking for the start of a block so wrap around */
			if (i == STRESS_TEST_NUM_BLOCKS) {
				i = 0;
			}

		} else { /* Look for the end of the block */

			/* If the end of the region is reached, this must be
			 * the end of the chunk as well.*/
			if (i == STRESS_TEST_NUM_BLOCKS) {
				end = i;
				break;
			}
		}

		/* Back to the starting point of the search: no chunk found */
		if (i == seek) {
			break;
		}
	}

	/* No chunks found */
	if ((start == -1) || (end == -1)) {
		return 0;
	}

	int blocks = end - start;

	bool is_correct_size = true;

	if ((rand() % 5) == 0) { /* Make it fail sometimes */
		blocks++;
		is_correct_size = false;
	}

	uintptr_t base_va = memory_base_va + start * block_size;
	size_t size = blocks * block_size;

	if (is_correct_size) {
		/* Remove, it should succeed */
		int rc = remove_region(base_va, size);
		if (rc != 0) {
			tftf_testcase_printf("%d: remove_region failed: %d\n",
					     __LINE__, rc);
			return 1;
		}

		/* Flag as unused */
		for (int i = start; i < start + blocks; i++) {
			block_used[i] = 0;
		}

		return 1;
	} else {
		/* Remove, it should fail */
		int rc = remove_region(base_va, size);
		if (rc == 0) {
			tftf_testcase_printf("%d: remove_region succeeded\n",
					     __LINE__);
			return 1;
		}

		return 0;
	}
}

/* Returns number of allocated chunks */
static int get_num_chunks(void)
{
	int count = 0;

	for (int i = 0; i < STRESS_TEST_NUM_BLOCKS; i++) {
		if (block_used[i] == 2) {
			count++;
		}
	}

	return count;
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

#ifdef __aarch64__
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

/**
 * @Test_Aim@ Perform dynamic translation tables API alignment tests
 *
 * This test makes sure that the alloc VA APIs return addresses aligned as
 * expected.
 */
test_result_t xlat_lib_v2_alignment_test(void)
{
	uintptr_t memory_base_va;
	unsigned long long memory_base_pa;
	int rc, i;

	/*
	 * 1) Try to allocate an invalid region. It should fail, but it will
	 * return the address of memory that can be used for the following
	 * tests.
	 */

	rc = add_region_alloc_va(0, &memory_base_va, SIZE_MAX, MT_DEVICE);
	if (rc == 0) {
		tftf_testcase_printf("%d: add_region_alloc_va() didn't fail\n",
				     __LINE__);

		return TEST_RESULT_FAIL;
	}

	/*
	 * Get address of memory region over the max used VA that is aligned to
	 * a L1 block for the next tests.
	 */
	memory_base_pa = (memory_base_va + SIZE_L1 - 1ULL) & ~MASK_L1;

	INFO("Using 0x%llx as base address for tests.\n", memory_base_pa);

	/*
	 * 2) Try to allocate regions that have an unaligned base VA or PA, or
	 * a size that isn't multiple of PAGE_SIZE.
	 */

	rc = add_region(memory_base_va + 1, memory_base_va, PAGE_SIZE, MT_DEVICE);
	if (rc != -EINVAL) {
		tftf_testcase_printf("%d: add_region: %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	rc = add_region(memory_base_va, memory_base_va + 1, PAGE_SIZE, MT_DEVICE);
	if (rc != -EINVAL) {
		tftf_testcase_printf("%d: add_region: %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	rc = add_region(memory_base_va, memory_base_va, PAGE_SIZE + 1, MT_DEVICE);
	if (rc != -EINVAL) {
		tftf_testcase_printf("%d: add_region: %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

#ifdef __aarch64__
	/*
	 * 3) Try to allocate at least 1 GB aligned. There is only room for this
	 * in AArch64.
	 */

	rc = add_region_alloc_va(memory_base_pa, &memory_base_va, SIZE_L1,
				 MT_DEVICE);
	if (rc == -ENOMEM) {
		tftf_testcase_printf("%d: Not enough memory\n", __LINE__);
		return TEST_RESULT_FAIL;
	} else if (rc != 0) {
		tftf_testcase_printf("%d: add_region_alloc_va: %d\n",
				     __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	rc = remove_region(memory_base_va, SIZE_L1);
	if (rc != 0) {
		tftf_testcase_printf("%d: remove_region: %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}
#else
	/*
	 * 4) Try to allocate an absurdly large amount of misaligned memory,
	 * which should fail. In AArch64 there's enough memory to map 4GB of
	 * virtual memory so skip it.
	 */
	rc = add_region_alloc_va(memory_base_pa + PAGE_SIZE, &memory_base_va,
		 PLAT_VIRT_ADDR_SPACE_SIZE - (memory_base_pa + PAGE_SIZE),
		 MT_DEVICE);
	if (rc != -ENOMEM) {
		tftf_testcase_printf("%d: add_region_alloc_va: %d\n",
				     __LINE__, rc);
		return TEST_RESULT_FAIL;
	}
#endif

	/*
	 * 5) Try to allocate hand-picked regions of different sizes and make
	 * sure that the resulting address is aligned to the correct boundary.
	 */

	for (i = 0; i < ARRAY_SIZE(mem_tests); i++) {
		/* Allocate to a correct PA boundary. */

		unsigned long long base_pa = memory_base_pa;

		rc = add_region_alloc_va(base_pa, &memory_base_va,
					 mem_tests[i].size, MT_DEVICE);
		if ((rc == -ENOMEM) || (rc == -ERANGE)) {
			/*
			 * Skip regions that are too big for the address space.
			 * This is a problem specially in AArch32, when the max
			 * virtual address space width is 32 bit.
			 */
			WARN("%d: Not enough memory for case %d\n",
			     __LINE__, i);
			continue;
		} else if (rc != 0) {
			tftf_testcase_printf("%d: add_region_alloc_va: %d\n",
					     __LINE__, rc);
			return TEST_RESULT_FAIL;
		}

		rc = remove_region(memory_base_va, mem_tests[i].size);
		if (rc != 0) {
			tftf_testcase_printf("%d: remove_region: %d\n",
					     __LINE__, rc);
			return TEST_RESULT_FAIL;
		}

		if (memory_base_va & mem_tests[i].expected_va_mask) {
			tftf_testcase_printf("%d: Invalid alignment for case %d\n",
					     __LINE__, i);
			return TEST_RESULT_FAIL;
		}

		/*
		 * Try to allocate to an incorrect PA boundary (a smaller one).
		 * This only makes sense for regions that are aligned to
		 * boundaries bigger than 4 KB, as there cannot be an incorrect
		 * alignment for 4 KB aligned regions.
		 */

		if (mem_tests[i].expected_va_mask != MASK_L3) {

			base_pa = memory_base_pa;

			if (mem_tests[i].expected_va_mask == MASK_L1) {
				base_pa += SIZE_L2;
			} else if (mem_tests[i].expected_va_mask == MASK_L2) {
				base_pa += SIZE_L3;
			}

			rc = add_region_alloc_va(base_pa, &memory_base_va,
						 mem_tests[i].size, MT_DEVICE);
			if (rc == 0) {

				rc = remove_region(memory_base_va,
						   mem_tests[i].size);
				if (rc != 0) {
					tftf_testcase_printf("%d: remove_region: %d\n",
							     __LINE__, rc);
					return TEST_RESULT_FAIL;
				}

			} else if ((rc != -ENOMEM) && (rc != -ERANGE)) {
				/*
				 * It could happen that we run out of memory, so
				 * it doesn't make sense to fail because of
				 * that. However, any other error is a
				 * legitimate error.
				 */
				tftf_testcase_printf("%d: add_region_alloc_va: %d\n",
						     __LINE__, rc);
				return TEST_RESULT_FAIL;
			}
		}
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * @Test_Aim@ Perform dynamic translation tables API stress tests
 *
 * This test performs a stress test in the library APIs.
 */
test_result_t xlat_lib_v2_stress_test(void)
{
	test_result_t test_result = TEST_RESULT_SUCCESS;
	uintptr_t memory_base;
	int rc;

	/*
	 * 1) Try to allocate an invalid region. It should fail, but it will
	 * return the address of memory that can be used for the following
	 * tests.
	 */
	rc = add_region_alloc_va(0, &memory_base, SIZE_MAX, MT_DEVICE);
	if (rc == 0) {
		tftf_testcase_printf("%d: add_region_alloc_va() didn't fail\n",
				     __LINE__);
		return TEST_RESULT_FAIL;
	}

	/*
	 * Get address of memory region over the max used VA that is aligned to
	 * a L1 block for the next tests.
	 */
	memory_base = (memory_base + SIZE_L1 - 1UL) & ~MASK_L1;

	INFO("Using 0x%lx as base address for tests.\n", memory_base);

	/* 2) Get a region of memory that we can use for testing. */

	/*
	 * Try with blocks 64 times the size of a page and reduce the size until
	 * it fits. PAGE_SIZE can only be 4, 16 or 64KB.
	 */
	block_size = PAGE_SIZE * 64;
	for (;;) {
		memory_size = block_size * STRESS_TEST_NUM_BLOCKS;
		rc = add_region(memory_base, memory_base, memory_size,
				MT_DEVICE);
		if (rc == 0) {
			break;
		}

		block_size >>= 1;
		if (block_size < PAGE_SIZE) {
			tftf_testcase_printf("%d: Couldn't allocate enough memory\n",
					     __LINE__);
			return TEST_RESULT_FAIL;
		}
	}

	rc = remove_region(memory_base, memory_size);
	if (rc != 0) {
		tftf_testcase_printf("%d: remove_region: %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	/* 3) Start stress test with the calculated top VA and space */

	memset(block_used, 0, sizeof(block_used));

	for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
		if ((rand() % 4) > 0) {
			rc = alloc_random_chunk();
		} else {
			rc = free_random_chunk();
		}

		if (rc == -1) {
			test_result = TEST_RESULT_FAIL;
			break;
		}
	}

	/* Cleanup of regions left allocated by the stress test */
	while (get_num_chunks() > 0) {
		rc = free_random_chunk();
		if (rc == -1) {
			test_result = TEST_RESULT_FAIL;
		}
	}

	return test_result;
}
