/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psci.h>
#include <stdlib.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <xlat_tables_defs.h>
#include <xlat_tables_v2.h>

#define SENTINEL		0x55
#define INVALID_ARCH_RESET	0x00000001
#define INVALID_VENDOR_RESET	0x80000002
#define MEM_PROTECT_ENABLE	1
#define MEM_PROTECT_DISABLE	0


/*
 * Test warm reset using PSCI RESET2 call (parameter 0)
 * Returns:
 * 	TEST_RESULT_SUCCESS: The system resets after calling RESET2
 *	TEST_RESULT_FAIL: The RESET2 PSCI call failed
 */
static test_result_t reset2_warm_helper(void *arg)
{
	smc_args args = { SMC_PSCI_RESET2, 0};
	unsigned char *sentinel = arg;
	unsigned char value;

	assert(sentinel != NULL);

	if (tftf_is_rebooted()) {
		value = *sentinel;
		if (value != SENTINEL) {
			tftf_testcase_printf("Sentinel address modified\n");
			return TEST_RESULT_FAIL;
		}
		return TEST_RESULT_SUCCESS;
	}
	*sentinel = SENTINEL;

	tftf_notify_reboot();
	tftf_smc(&args);

	/* The PSCI RESET2 call is not supposed to return */
	tftf_testcase_printf("System didn't shutdown properly\n");
	return TEST_RESULT_FAIL;
}

test_result_t reset2_warm(void)
{
	map_args_unmap_t args;
	unsigned char *sentinel;
	int ret;

	ret = tftf_get_psci_feature_info(SMC_PSCI_RESET2);
	if (ret == PSCI_E_NOT_SUPPORTED) {
		tftf_testcase_printf("PSCI RESET2 is not supported %d\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	sentinel = psci_mem_prot_get_sentinel();
	if (sentinel == NULL) {
		tftf_testcase_printf("Could not find a suitable address for the sentinel.\n");
		return TEST_RESULT_SKIPPED;
	}

	args.addr = (uintptr_t) sentinel & ~PAGE_SIZE_MASK;
	args.size = PAGE_SIZE;
	args.attr = MT_RW_DATA;
	args.arg = sentinel;

	return map_test_unmap(&args, reset2_warm_helper);
}

/*
 * Test correct error handling
 * Returns:
 * 	TEST_RESULT_SUCCESS: If the system catches all the wrong calls
 *	TEST_RESULT_FAIL: Some PSCI call failed
 */
test_result_t reset2_test_invalid(void)
{
	smc_args args = {SMC_PSCI_RESET2};
	smc_ret_values ret_vals;
	int ret;

	ret = tftf_get_psci_feature_info(SMC_PSCI_RESET2);
	if (ret == PSCI_E_NOT_SUPPORTED)
		return TEST_RESULT_SKIPPED;

	args.arg1 = INVALID_VENDOR_RESET;
	ret_vals = tftf_smc(&args);
	ret = ret_vals.ret0;

	if (ret >= 0)
		return TEST_RESULT_FAIL;

	args.arg1 = INVALID_ARCH_RESET;
	ret_vals = tftf_smc(&args);
	ret = ret_vals.ret0;

	if (ret >= 0)
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
}

/*
 * Test combination of reset2 and mem_protect
 * Returns:
 *	TEST_RESULT_SUCCESS: if the system is reseted and mem_protect
 *	                     is disabled.
 *	TEST_RESULT_FAIL: Some PSCI call failed or mem_protect wasn't
 *	                  disabled
 */
static test_result_t reset2_mem_protect_helper(void *arg)
{
	int ret;
	unsigned char value;
	smc_args args = { SMC_PSCI_RESET2, 0};
	unsigned char *sentinel = arg;

	assert(sentinel != NULL);

	ret = tftf_get_psci_feature_info(SMC_PSCI_MEM_PROTECT);
	if (ret == PSCI_E_NOT_SUPPORTED)
		return TEST_RESULT_SKIPPED;

	if (tftf_is_rebooted()) {
		if (psci_mem_protect(0) != MEM_PROTECT_DISABLE) {
			tftf_testcase_printf("mem_protect is not disabled");
			return TEST_RESULT_SUCCESS;
		}
		value = *sentinel;
		if (value != SENTINEL) {
			tftf_testcase_printf("Sentinel address modified\n");
			return TEST_RESULT_FAIL;
		}
		return TEST_RESULT_SUCCESS;
	}

	*sentinel = SENTINEL;

	ret = psci_mem_protect(0);
	if (ret != MEM_PROTECT_ENABLE && ret != MEM_PROTECT_DISABLE) {
		tftf_testcase_printf("error calling mem_protect");
		return TEST_RESULT_FAIL;
	}

	tftf_notify_reboot();
	tftf_smc(&args);

	/* The PSCI RESET2 call is not supposed to return */
	tftf_testcase_printf("System didn't shutdown properly\n");
	return TEST_RESULT_FAIL;
}

test_result_t reset2_mem_protect(void)
{
	map_args_unmap_t args;
	unsigned char *sentinel;
	int ret;

	ret = tftf_get_psci_feature_info(SMC_PSCI_RESET2);
	if (ret == PSCI_E_NOT_SUPPORTED) {
		tftf_testcase_printf("PSCI RESET2 is not supported %d\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	sentinel = psci_mem_prot_get_sentinel();
	if (sentinel == NULL) {
		tftf_testcase_printf("Could not find a suitable address for the sentinel.\n");
		return TEST_RESULT_SKIPPED;
	}

	args.addr = (uintptr_t) sentinel & ~PAGE_SIZE_MASK;
	args.size = PAGE_SIZE;
	args.attr = MT_RW_DATA;
	args.arg = sentinel;

	return map_test_unmap(&args, reset2_mem_protect_helper);
}
