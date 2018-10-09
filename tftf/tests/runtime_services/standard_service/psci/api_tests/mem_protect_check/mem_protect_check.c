/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>
#include <psci.h>
#include <stdlib.h>
#include <test_helpers.h>
#include <tftf_lib.h>

static test_result_t mem_prot_check(uintptr_t addr, size_t size, int expected)
{
	int ret;

	ret = psci_mem_protect_check(addr, size);
	if (ret != expected) {
		tftf_testcase_printf("MEM_PROTEC_CHECK failed in (%llx,%llx)\n",
				     (unsigned long long) addr,
				     (unsigned long long) size);
		return 0;
	}
	return 1;
}

static test_result_t test_region(const mem_region_t *region)
{
	uintptr_t max_addr = region->addr + region->size;

	if (!mem_prot_check(region->addr, 0, PSCI_E_DENIED))
		return 0;
	if (!mem_prot_check(region->addr, SIZE_MAX, PSCI_E_DENIED))
		return 0;
	if (!mem_prot_check(region->addr, 1, PSCI_E_SUCCESS))
		return 0;
	if (!mem_prot_check(region->addr, 1, PSCI_E_SUCCESS))
		return 0;
	if (!mem_prot_check(region->addr, region->size - 1, PSCI_E_SUCCESS))
		return 0;
	if (!mem_prot_check(max_addr - 1, 1, PSCI_E_SUCCESS))
		return 0;
	return 1;
}

/*
 * Test to verify that mem_protect_check_range returns correct answer
 * for known memory locations.
 *
 * Returns:
 *         TEST_RESULT_SUCCESS : When all the checks return the expected value
 *         TEST_RESULT_FAIL : when some check fails or return an unepected value
 */
test_result_t test_mem_protect_check(void)
{
	int ret, nregions;
	const mem_region_t *regions;

	ret = tftf_get_psci_feature_info(SMC_PSCI_MEM_PROTECT_CHECK);
	if (ret == PSCI_E_NOT_SUPPORTED) {
		tftf_testcase_printf("MEM_PROTECT_CHECK is not supported\n");
		return TEST_RESULT_SKIPPED;
	}

	regions = plat_get_prot_regions(&nregions);
	if (nregions <= 0) {
		tftf_testcase_printf("Platform doesn't define testcases for MEM_PROTECT_CHECK\n");
		return TEST_RESULT_SKIPPED;
	}

	if (!mem_prot_check(UINTPTR_MAX, 1, PSCI_E_DENIED))
		return TEST_RESULT_FAIL;

	if (!mem_prot_check(1, SIZE_MAX, PSCI_E_DENIED))
		return TEST_RESULT_FAIL;

	if (!mem_prot_check(UINTPTR_MAX, 0, PSCI_E_DENIED))
		return TEST_RESULT_FAIL;

	if (!mem_prot_check(0, 1, PSCI_E_DENIED))
		return TEST_RESULT_FAIL;

	while (nregions-- > 0) {
		if (!test_region(regions++))
			return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
