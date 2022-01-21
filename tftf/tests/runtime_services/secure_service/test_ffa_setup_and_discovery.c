/*
 * Copyright (c) 2020-2022, Arm Limited. All rights reserved.
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
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}, {IVY_UUID}
	};
static const struct ffa_uuid null_uuid = { .uuid = {0} };

static const struct ffa_partition_info ffa_expected_partition_info[] = {
	/* Primary partition info */
	{
		.id = SP_ID(1),
		.exec_context = PRIMARY_EXEC_CTX_COUNT,
		.properties = FFA_PARTITION_DIRECT_REQ_RECV |
			      FFA_PARTITION_NOTIFICATION,
		.uuid = sp_uuids[0]
	},
	/* Secondary partition info */
	{
		.id = SP_ID(2),
		.exec_context = SECONDARY_EXEC_CTX_COUNT,
		.properties = FFA_PARTITION_DIRECT_REQ_RECV |
			      FFA_PARTITION_NOTIFICATION,
		.uuid = sp_uuids[1]
	},
	/* Tertiary partition info */
	{
		.id = SP_ID(3),
		.exec_context = TERTIARY_EXEC_CTX_COUNT,
		.properties = FFA_PARTITION_DIRECT_REQ_RECV |
			      FFA_PARTITION_NOTIFICATION,
		.uuid = sp_uuids[2]
	},
	/* Ivy partition info */
	{
		.id = SP_ID(4),
		.exec_context = IVY_EXEC_CTX_COUNT,
		.properties = FFA_PARTITION_DIRECT_REQ_RECV,
		.uuid = sp_uuids[3]
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
 */
test_result_t test_ffa_rxtx_map_success(void)
{
	return test_ffa_rxtx_map(FFA_SUCCESS_SMC32);
}

/**
 * Test to verify that 2nd call to FFA_RXTX_MAP should fail.
 */
test_result_t test_ffa_rxtx_map_fail(void)
{
	INFO("This test expects error log.\n");
	return test_ffa_rxtx_map(FFA_ERROR);
}

static test_result_t test_ffa_rxtx_unmap(uint32_t expected_return)
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

	ret = ffa_rxtx_unmap();
	if (!is_expected_ffa_return(ret, expected_return)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * Test unmapping RXTX buffers from NWd.
 */
test_result_t test_ffa_rxtx_unmap_success(void)
{
	return test_ffa_rxtx_unmap(FFA_SUCCESS_SMC32);
}

/**
 * Test to verify that 2nd call to FFA_RXTX_UNMAP should fail.
 */
test_result_t test_ffa_rxtx_unmap_fail(void)
{
	INFO("This test expects error log.\n");
	return test_ffa_rxtx_unmap(FFA_ERROR);
}

/**
 * Test mapping RXTX buffers that have been previously unmapped from NWd.
 * This test also sets the Mailbox for other SPM related tests that need to use
 * RXTX buffers.
 */
test_result_t test_ffa_rxtx_map_unmapped_success(void)
{
	test_result_t ret =  test_ffa_rxtx_map(FFA_SUCCESS_SMC32);

	if (ret == TEST_RESULT_SUCCESS) {
		INFO("Set RXTX Mailbox for remaining spm tests.\n");
		set_tftf_mailbox(&mb);
	}
	return ret;
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
	CHECK_SPMC_TESTING_SETUP(1, 1, sp_uuids);

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
		ffa_expected_partition_info,
		ARRAY_SIZE(ffa_expected_partition_info))) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * Attempt to get v1.0 partition info descriptors.
 */
test_result_t test_ffa_partition_info_v1_0(void)
{
	/**************************************************************
	 * Check if SPMC has ffa_version and expected FFA endpoints
	 * are deployed.
	 *************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, sp_uuids);

	GET_TFTF_MAILBOX(mb);

	test_result_t result = TEST_RESULT_SUCCESS;
	smc_ret_values ret = ffa_partition_info_get(null_uuid);
	uint64_t expected_size = ARRAY_SIZE(ffa_expected_partition_info);

	if (ffa_func_id(ret) == FFA_SUCCESS_SMC32) {
		if (ffa_partition_info_count(ret) != expected_size) {
			ERROR("Unexpected number of partitions %d\n",
			      ffa_partition_info_count(ret));
			return TEST_RESULT_FAIL;
		}
		const struct ffa_partition_info_v1_0 *info =
			(const struct ffa_partition_info_v1_0 *)(mb.recv);

		for (unsigned int i = 0U; i < expected_size; i++) {
			if (info[i].id != ffa_expected_partition_info[i].id) {
				ERROR("Wrong ID. Expected %x, got %x\n",
				      ffa_expected_partition_info[i].id,
				      info[i].id);
				result = TEST_RESULT_FAIL;
			}
			if (info[i].exec_context !=
			    ffa_expected_partition_info[i].exec_context) {
				ERROR("Wrong context. Expected %d, got %d\n",
				      ffa_expected_partition_info[i].exec_context,
				      info[i].exec_context);
				result = TEST_RESULT_FAIL;
			}
			if (info[i].properties !=
			    ffa_expected_partition_info[i].properties) {
				ERROR("Wrong properties. Expected %d, got %d\n",
				      ffa_expected_partition_info[i].properties,
				      info[i].properties);
				result = TEST_RESULT_FAIL;
			}
		}
	}

	ret = ffa_rx_release();
	if (is_ffa_call_error(ret)) {
		ERROR("Failed to release RX buffer\n");
		result = TEST_RESULT_FAIL;
	}
	return result;
}
