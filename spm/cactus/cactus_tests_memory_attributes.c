/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <platform_def.h>
#include <secure_partition.h>
#include <sp_helpers.h>
#include <sprt_svc.h>
#include <stdio.h>
#include <types.h>
#include <xlat_tables_defs.h>

#include "cactus.h"
#include "cactus_def.h"
#include "cactus_tests.h"

/* This is filled at runtime. */
static uintptr_t cactus_tests_start;
static uintptr_t cactus_tests_end;
static uintptr_t cactus_tests_size;

/*
 * Send an SPRT_MEMORY_PERM_ATTR_SET_AARCH64 SVC with the given arguments.
 * Return the return value of the SVC.
 */
static int32_t request_mem_attr_changes(uintptr_t base_address,
					int pages_count,
					uint32_t memory_access_controls)
{
	INFO("Requesting memory attributes change\n");
	INFO("  Start address  : %p\n", (void *) base_address);
	INFO("  Number of pages: %i\n", pages_count);
	INFO("  Attributes     : 0x%x\n", memory_access_controls);

	svc_args svc_values = { SPRT_MEMORY_PERM_ATTR_SET_AARCH64,
				base_address,
				pages_count,
				memory_access_controls };
	return sp_svc(&svc_values);
}

/*
 * Send an SPRT_MEMORY_PERM_ATTR_GET_AARCH64 SVC with the given arguments.
 * Return the return value of the SVC.
 */
static int32_t request_get_mem_attr(uintptr_t base_address)
{
	INFO("Requesting memory attributes\n");
	INFO("  Base address  : %p\n", (void *) base_address);

	svc_args svc_values = { SPRT_MEMORY_PERM_ATTR_GET_AARCH64,
				base_address };
	return sp_svc(&svc_values);
}

/*
 * This function expects a base address and number of pages identifying the
 * extents of some memory region mapped as non-executable, read-only.
 *
 * 1) It changes its data access permissions to read-write.
 * 2) It checks this memory can now be written to.
 * 3) It restores the original data access permissions.
 *
 * If any check fails, it loops forever. It could also trigger a permission
 * fault while trying to write to the memory.
 */
static void mem_attr_changes_unittest(uintptr_t addr, int pages_count)
{
	int32_t ret;
	uintptr_t end_addr = addr + pages_count * PAGE_SIZE;
	uint32_t old_attr, new_attr;

	char test_desc[50];

	snprintf(test_desc, sizeof(test_desc),
		 "RO -> RW (%i page(s) from address 0x%lx)", pages_count, addr);
	announce_test_start(test_desc);

	/*
	 * Ensure we don't change the attributes of some random memory
	 * location
	 */
	assert(addr >= cactus_tests_start);
	assert(end_addr < (cactus_tests_start + cactus_tests_size));

	old_attr = SPRT_MEMORY_PERM_ATTR_RO;
	/* Memory was read-only, let's try changing that to RW */
	new_attr = SPRT_MEMORY_PERM_ATTR_RW;

	ret = request_mem_attr_changes(addr, pages_count, new_attr);
	expect(ret, SPRT_SUCCESS);
	printf("Successfully changed memory attributes\n");

	/* The attributes should be the ones we have just written. */
	ret = request_get_mem_attr(addr);
	expect(ret, SPRT_SUCCESS | (new_attr << SPRT_MEMORY_PERM_ATTR_SHIFT));

	/* If it worked, we should be able to write to this memory now! */
	for (unsigned char *data = (unsigned char *) addr;
	     (uintptr_t) data != end_addr;
	     ++data) {
		*data = 42;
	}
	printf("Successfully wrote to the memory\n");

	/* Let's revert back to the original attributes for the next test */
	ret = request_mem_attr_changes(addr, pages_count, old_attr);
	expect(ret, SPRT_SUCCESS);
	printf("Successfully restored the old attributes\n");

	/* The attributes should be the original ones again. */
	ret = request_get_mem_attr(addr);
	expect(ret, SPRT_SUCCESS | (old_attr << SPRT_MEMORY_PERM_ATTR_SHIFT));

	announce_test_end(test_desc);
}

/*
 * Exercise the ability of the Trusted Firmware to change the data access
 * permissions and instruction execution permissions of some memory region.
 */
void mem_attr_changes_tests(void)
{
	uint32_t attributes;
	int32_t ret;
	uintptr_t addr;

	cactus_tests_start = CACTUS_TEST_MEM_BASE;
	cactus_tests_size  = CACTUS_TEST_MEM_SIZE;
	cactus_tests_end   = cactus_tests_start + cactus_tests_size;

	const char *test_sect_desc = "memory attributes changes";

	announce_test_section_start(test_sect_desc);
	/*
	 * Start with error cases, i.e. requests that are expected to be denied
	 */
	const char *test_desc1 = "Reserved attributes value";

	announce_test_start(test_desc1);
	attributes = U(3);
	ret = request_mem_attr_changes(cactus_tests_start, 1, attributes);
	expect(ret, SPRT_INVALID_PARAMETER);
	announce_test_end(test_desc1);

	const char *test_desc2 = "Size == 0";

	announce_test_start(test_desc2);
	attributes = SPRT_MEMORY_PERM_ATTR_RW;
	ret = request_mem_attr_changes(cactus_tests_start, 0, attributes);
	expect(ret, SPRT_INVALID_PARAMETER);
	announce_test_end(test_desc2);

	const char *test_desc3 = "Unaligned address";

	announce_test_start(test_desc3);
	attributes = SPRT_MEMORY_PERM_ATTR_RW;
	/* Choose an address not aligned to a page boundary. */
	addr = cactus_tests_start + 5;
	ret = request_mem_attr_changes(addr, 1, attributes);
	expect(ret, SPRT_INVALID_PARAMETER);
	announce_test_end(test_desc3);

	const char *test_desc4 = "Unmapped memory region";

	announce_test_start(test_desc4);
	addr = cactus_tests_end + 2 * PAGE_SIZE;
	attributes = SPRT_MEMORY_PERM_ATTR_RW;
	ret = request_mem_attr_changes(addr, 3, attributes);
	expect(ret, SPRT_INVALID_PARAMETER);
	announce_test_end(test_desc4);

	const char *test_desc5 = "Partially unmapped memory region";

	announce_test_start(test_desc5);
	addr = cactus_tests_end - 2 * PAGE_SIZE;
	attributes = SPRT_MEMORY_PERM_ATTR_RW;
	ret = request_mem_attr_changes(addr, 6, attributes);
	expect(ret, SPRT_INVALID_PARAMETER);
	announce_test_end(test_desc5);

	const char *test_desc6 = "Memory region mapped with the wrong granularity";

	announce_test_start(test_desc6);
	/*
	 * This address is usually mapped at a 2 MiB granularity. By using as
	 * test address the block after the console we make sure that in case
	 * the attributes of the block actually changed, the console would work
	 * and we would get the error message.
	 */
	addr = ((uintptr_t)PLAT_ARM_UART_BASE + 0x200000ULL) & ~(0x200000ULL - 1ULL);
	attributes = SPRT_MEMORY_PERM_ATTR_RW;
	ret = request_mem_attr_changes(addr, 1, attributes);
	expect(ret, SPRT_INVALID_PARAMETER);
	announce_test_end(test_desc6);

	const char *test_desc7 = "Try some valid memory change requests";

	announce_test_start(test_desc7);
	for (unsigned int i = 0; i < 20; ++i) {
		/*
		 * Choose some random address in the pool of memory reserved
		 * for these tests.
		 */
		const int pages_max = cactus_tests_size / PAGE_SIZE;
		int pages_count = bound_rand(1, pages_max);

		addr = bound_rand(
			cactus_tests_start,
			cactus_tests_end - (pages_count * PAGE_SIZE));
		/* Align to PAGE_SIZE. */
		addr &= ~(PAGE_SIZE - 1);

		mem_attr_changes_unittest(addr, pages_count);
	}
	announce_test_end(test_desc7);

	announce_test_section_end(test_sect_desc);
}
