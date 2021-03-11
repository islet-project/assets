/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>

#include <test_helpers.h>
#include <xlat_tables_defs.h>

static struct mailbox_buffers mb;

static test_result_t test_ffa_rxtx_map(uint32_t expected_return)
{
	smc_ret_values ret;

	/**********************************************************************
	 * Verify that FFA is there and that it has the correct version.
	 **********************************************************************/
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 0);

	/**********************************************************************
	 * If OP-TEE is SPMC skip this test.
	 **********************************************************************/
	if (check_spmc_execution_level()) {
		VERBOSE("OP-TEE as SPMC at S-EL1. Skipping test!\n");
		return TEST_RESULT_SKIPPED;
	}

	/*
	 * Declare RXTX buffers, assign them to the mailbox and call
	 * FFA_RXTX_MAP.
	 */
	CONFIGURE_AND_MAP_MAILBOX(mb, PAGE_SIZE, ret);
	if (ffa_func_id(ret) != expected_return) {
		ERROR("Failed to map RXTX buffers %x!\n", ffa_error_code(ret));
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * Test mapping RXTX buffers from NWd.
 * This test also sets the Mailbox for other SPM related tests that need to use
 * RXTX buffers.
 */
test_result_t test_ffa_rxtx_map_success(void)
{
	test_result_t ret = test_ffa_rxtx_map(FFA_SUCCESS_SMC32);

	if (ret == TEST_RESULT_SUCCESS) {
		INFO("Set RXTX Mailbox for remaining spm tests!\n");
		set_tftf_mailbox(&mb);
	}
	return ret;
}

/**
 * Test to verify that 2nd call to FFA_RXTX_MAP should fail.
 */
test_result_t test_ffa_rxtx_map_fail(void)
{
	INFO("This test expects error log.\n");
	return test_ffa_rxtx_map(FFA_ERROR);
}
