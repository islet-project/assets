/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>

#include <ffa_endpoints.h>
#include <ffa_helpers.h>
#include <ffa_svc.h>
#include <spm_common.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <xlat_tables_defs.h>

static bool should_skip_version_test;

static struct mailbox_buffers mb;

static const struct ffa_uuid sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}
	};
static const struct ffa_uuid null_uuid = { .uuid = {0} };

static const struct ffa_partition_info ffa_expected_partition_info[] = {
	/* Primary partition info */
	{
		.id = SP_ID(1),
		.exec_context = PRIMARY_EXEC_CTX_COUNT,
		.properties = FFA_PARTITION_DIRECT_REQ_RECV
	},
	/* Secondary partition info */
	{
		.id = SP_ID(2),
		.exec_context = SECONDARY_EXEC_CTX_COUNT,
		.properties = FFA_PARTITION_DIRECT_REQ_RECV
	},
	/* Tertiary partition info */
	{
		.id = SP_ID(3),
		.exec_context = TERTIARY_EXEC_CTX_COUNT,
		.properties = FFA_PARTITION_DIRECT_REQ_RECV
	},
	/* Ivy partition info */
	{
		.id = SP_ID(4),
		.exec_context = IVY_EXEC_CTX_COUNT,
		.properties = FFA_PARTITION_DIRECT_REQ_RECV
	}
};

/*
 * Using FFA version expected for SPM.
 */
#define SPM_VERSION MAKE_FFA_VERSION(FFA_VERSION_MAJOR, FFA_VERSION_MINOR)

/******************************************************************************
 * FF-A Features ABI Tests
 ******************************************************************************/

test_result_t test_ffa_features(void)
{
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 0);

	/* Check if SPMC is OP-TEE at S-EL1 */
	if (check_spmc_execution_level()) {
		/* FFA_FEATURES is not yet supported in OP-TEE */
		return TEST_RESULT_SUCCESS;
	}

	smc_ret_values ffa_ret;
	unsigned int expected_ret;
	const struct ffa_features_test *ffa_feature_test_target;
	unsigned int i, test_target_size =
		get_ffa_feature_test_target(&ffa_feature_test_target);
	struct ffa_features_test test_target;

	for (i = 0U; i < test_target_size; i++) {
		test_target = ffa_feature_test_target[i];
		ffa_ret = ffa_features(test_target.feature);
		expected_ret = FFA_VERSION_COMPILED
			>= test_target.version_added ?
			test_target.expected_ret : FFA_ERROR;
		if (ffa_func_id(ffa_ret) != expected_ret) {
			tftf_testcase_printf("%s returned %x, expected %x\n",
					test_target.test_name,
					ffa_func_id(ffa_ret),
					expected_ret);
			return TEST_RESULT_FAIL;
		}
		if ((expected_ret == FFA_ERROR) &&
				(ffa_error_code(ffa_ret) != FFA_ERROR_NOT_SUPPORTED)) {
			tftf_testcase_printf("%s failed for the wrong reason: "
					"returned %x, expected %x\n",
					test_target.test_name,
					ffa_error_code(ffa_ret),
					FFA_ERROR_NOT_SUPPORTED);
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}

/******************************************************************************
 * FF-A Version ABI Tests
 ******************************************************************************/

/*
 * Calls FFA Version ABI, and checks if the result as expected.
 */
static test_result_t test_ffa_version(uint32_t input_version,
					uint32_t expected_return)
{
	if (should_skip_version_test) {
		return TEST_RESULT_SKIPPED;
	}

	smc_ret_values ret_values = ffa_version(input_version);

	uint32_t spm_version = (uint32_t)(0xFFFFFFFF & ret_values.ret0);

	if (spm_version == expected_return) {
		return TEST_RESULT_SUCCESS;
	}

	tftf_testcase_printf("Input Version: 0x%x\n"
			     "Return: 0x%x\nExpected: 0x%x\n",
			      input_version, spm_version, expected_return);

	return TEST_RESULT_FAIL;
}

/*
 * @Test_Aim@ Validate what happens when using same version as SPM.
 */
test_result_t test_ffa_version_equal(void)
{
	/*
	 * FFA_VERSION interface is used to check that SPM functionality is
	 * supported. On FFA_VERSION invocation from TFTF, the SPMD returns
	 * either NOT_SUPPORTED or the SPMC version value provided in the SPMC
	 * manifest. The variable "should_skip_test" is set to true when the
	 * SPMD returns NOT_SUPPORTED or a mismatched version, which means that
	 * a TFTF physical FF-A endpoint version (SPM_VERSION) does not match
	 * the SPMC's physical FF-A endpoint version. This prevents running the
	 * subsequent FF-A version tests (and break the test flow), as they're
	 * not relevant when the SPMD is not present within BL31
	 * (FFA_VERSION returns NOT_SUPPORTED).
	 */
	test_result_t ret = test_ffa_version(SPM_VERSION, SPM_VERSION);

	if (ret != TEST_RESULT_SUCCESS) {
		should_skip_version_test = true;
		ret = TEST_RESULT_SKIPPED;
	}
	return ret;
}

/*
 * @Test_Aim@ Validate what happens when setting bit 31 in
 * 'input_version'. As per spec, FFA version is 31 bits long.
 * Bit 31 set is an invalid input.
 */
test_result_t test_ffa_version_bit31(void)
{
	return test_ffa_version(FFA_VERSION_BIT31_MASK | SPM_VERSION,
				FFA_ERROR_NOT_SUPPORTED);
}

/*
 * @Test_Aim@ Validate what happens for bigger version than SPM's.
 */
test_result_t test_ffa_version_bigger(void)
{
	return test_ffa_version(MAKE_FFA_VERSION(FFA_VERSION_MAJOR + 1, 0),
				SPM_VERSION);
}

/*
 * @Test_Aim@ Validate what happens for smaller version than SPM's.
 */
test_result_t test_ffa_version_smaller(void)
{
	return test_ffa_version(MAKE_FFA_VERSION(0, 9), SPM_VERSION);
}

/******************************************************************************
 * FF-A RXTX ABI Tests
 ******************************************************************************/

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

/******************************************************************************
 * FF-A SPM_ID_GET ABI Tests
 ******************************************************************************/

test_result_t test_ffa_spm_id_get(void)
{
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 1);

	smc_ret_values ffa_ret = ffa_spm_id_get();

	if (is_ffa_call_error(ffa_ret)) {
		ERROR("FFA_SPM_ID_GET call failed! Error code: 0x%x\n",
			ffa_error_code(ffa_ret));
		return TEST_RESULT_FAIL;
	}

	/* Check the SPMC value given in the fvp_spmc_manifest is returned */
	ffa_id_t spm_id = ffa_endpoint_id(ffa_ret);

	if (spm_id != SPMC_ID) {
		ERROR("Expected SPMC_ID of 0x%x\n received: 0x%x\n",
			SPMC_ID, spm_id);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/******************************************************************************
 * FF-A PARTITION_INFO_GET ABI Tests
 ******************************************************************************/

/**
 * Attempt to get the SP partition information for individual partitions as well
 * as all secure partitions.
 */
test_result_t test_ffa_partition_info(void)
{
	/***********************************************************************
	 * Check if SPMC has ffa_version and expected FFA endpoints are deployed.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, sp_uuids);

	GET_TFTF_MAILBOX(mb);

	if (!ffa_partition_info_helper(&mb, sp_uuids[0],
		&ffa_expected_partition_info[0], 1)) {
		return TEST_RESULT_FAIL;
	}
	if (!ffa_partition_info_helper(&mb, sp_uuids[1],
		&ffa_expected_partition_info[1], 1)) {
		return TEST_RESULT_FAIL;
	}
	if (!ffa_partition_info_helper(&mb, sp_uuids[2],
		&ffa_expected_partition_info[2], 1)) {
		return TEST_RESULT_FAIL;
	}
	if (!ffa_partition_info_helper(&mb, null_uuid,
		ffa_expected_partition_info, 4)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}