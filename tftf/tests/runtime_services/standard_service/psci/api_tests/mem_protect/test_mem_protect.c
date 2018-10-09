/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <psci.h>
#include <stdlib.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <xlat_tables_v2.h>

#define SENTINEL		0x55
#define MEM_PROT_ENABLED	1
#define MEM_PROT_DISABLED	0
/*
 * Test to verify that mem_protect is executed in next boot after calling
 * the PSCI mem_protect function
 *
 * Returns:
 *         TEST_RESULT_SUCCESS : when after rebooting mem_protect is activated
 *                               and the sentinel is detected to have been reset.
 *         TEST_RESULT_FAIL : when some of the calls to mem_protect fails or
 *                            sentinel is not cleared after resetting.
 */
static test_result_t test_mem_protect_helper(void *arg)
{
	int ret;
	unsigned char value;
	unsigned char *sentinel = arg;

	assert(sentinel != NULL);

	if (tftf_is_rebooted()) {
		value = *sentinel;
		if (value != 0 && value != SENTINEL) {
			tftf_testcase_printf("Sentinel address modified out of mem_protect:%d\n",
				value);
			return TEST_RESULT_FAIL;
		}
		if (value == SENTINEL) {
			tftf_testcase_printf("Sentinel address not cleared by mem_protect\n");
			return TEST_RESULT_FAIL;
		}
		return TEST_RESULT_SUCCESS;
	}

	ret = psci_mem_protect(MEM_PROT_DISABLED);
	if (ret != MEM_PROT_ENABLED && ret != MEM_PROT_DISABLED) {
		INFO("Mem_protect failed %d\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* mem_protect mechanism should be disabled at this point */
	ret = psci_mem_protect(MEM_PROT_ENABLED);
	if (ret != MEM_PROT_DISABLED) {
		tftf_testcase_printf("Mem_protect failed %d\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* mem_protect mechanism should be enabled at this point */
	ret = psci_mem_protect(MEM_PROT_ENABLED);
	if (ret != MEM_PROT_ENABLED) {
		tftf_testcase_printf("Mem_protect failed %d\n", ret);
		return TEST_RESULT_FAIL;
	}

	*sentinel = SENTINEL;

	/* Notify that we are rebooting now. */
	tftf_notify_reboot();

	psci_system_reset();
	/*
	 * psci_reset shouldn't return
	 */
	return TEST_RESULT_FAIL;
}

test_result_t test_mem_protect(void)
{
	map_args_unmap_t args;
	unsigned char *sentinel;
	int ret;

	ret = tftf_get_psci_feature_info(SMC_PSCI_MEM_PROTECT);
	if (ret == PSCI_E_NOT_SUPPORTED) {
		tftf_testcase_printf("Mem_protect is not supported %d\n", ret);
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

	return map_test_unmap(&args, test_mem_protect_helper);
}
