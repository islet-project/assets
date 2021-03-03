/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <smccc.h>

#include <arch_helpers.h>
#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>
#include <ffa_svc.h>
#include <platform.h>
#include <test_helpers.h>

#define ECHO_VAL1 U(0xa0a0a0a0)
#define ECHO_VAL2 U(0xb0b0b0b0)
#define ECHO_VAL3 U(0xc0c0c0c0)

static const struct ffa_uuid expected_sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}
	};

static test_result_t send_cactus_echo_cmd(ffa_vm_id_t sender,
					  ffa_vm_id_t dest,
					  uint64_t value)
{
	smc_ret_values ret;
	ret = cactus_echo_send_cmd(sender, dest, value);

	if (!is_ffa_direct_response(ret)) {
		ERROR("Failed to send message. error: %x\n",
		      ffa_error_code(ret));
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) != CACTUS_SUCCESS ||
	    cactus_echo_get_val(ret) != value) {
		ERROR("Echo Failed!\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

test_result_t test_ffa_direct_messaging(void)
{
	test_result_t result;

	/**********************************************************************
	 * Check SPMC has ffa_version and expected FFA endpoints are deployed.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	/**********************************************************************
	 * Send a message to SP1 through direct messaging
	 **********************************************************************/
	result = send_cactus_echo_cmd(HYP_ID, SP_ID(1), ECHO_VAL1);
	if (result != TEST_RESULT_SUCCESS) {
		return result;
	}

	/**********************************************************************
	 * Send a message to SP2 through direct messaging
	 **********************************************************************/
	result = send_cactus_echo_cmd(HYP_ID, SP_ID(2), ECHO_VAL2);
	if (result != TEST_RESULT_SUCCESS) {
		return result;
	}

	/**********************************************************************
	 * Send a message to SP1 through direct messaging
	 **********************************************************************/
	result = send_cactus_echo_cmd(HYP_ID, SP_ID(1), ECHO_VAL3);

	return result;
}

/**
 * The 'send_cactus_req_echo_cmd' sends a CACTUS_REQ_ECHO_CMD to a cactus SP.
 * Handling this command, cactus should then send CACTUS_ECHO_CMD to
 * the specified SP according to 'echo_dest'. If the CACTUS_ECHO_CMD is resolved
 * successfully, cactus will reply to tftf with CACTUS_SUCCESS, or CACTUS_ERROR
 * otherwise.
 * For the CACTUS_SUCCESS response, the test returns TEST_RESULT_SUCCESS.
 */
static test_result_t send_cactus_req_echo_cmd(ffa_vm_id_t sender,
					      ffa_vm_id_t dest,
					      ffa_vm_id_t echo_dest,
					      uint64_t value)
{
	smc_ret_values ret;

	ret = cactus_req_echo_send_cmd(sender, dest, echo_dest, value);

	if (!is_ffa_direct_response(ret)) {
		ERROR("Failed to send message. error: %x\n",
		      ffa_error_code(ret));
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) == CACTUS_ERROR) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

test_result_t test_ffa_sp_to_sp_direct_messaging(void)
{
	test_result_t result;

	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	result = send_cactus_req_echo_cmd(HYP_ID, SP_ID(1), SP_ID(2),
					  ECHO_VAL1);
	if (result != TEST_RESULT_SUCCESS) {
		return result;
	}

	/*
	 * The following the tests are intended to test the handling of a
	 * direct message request with a VM's ID as a the sender.
	 */
	result = send_cactus_req_echo_cmd(HYP_ID + 1, SP_ID(2), SP_ID(3),
					  ECHO_VAL2);
	if (result != TEST_RESULT_SUCCESS) {
		return result;
	}

	result = send_cactus_req_echo_cmd(HYP_ID + 2, SP_ID(3), SP_ID(1),
					  ECHO_VAL3);

	return result;
}

test_result_t test_ffa_sp_to_sp_deadlock(void)
{
	smc_ret_values ret;

	/**********************************************************************
	 * Check SPMC has ffa_version and expected FFA endpoints are deployed.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	ret = cactus_req_deadlock_send_cmd(HYP_ID, SP_ID(1), SP_ID(2), SP_ID(3));

	if (!is_ffa_direct_response(ret)) {
		ERROR("Failed to send message. error: %x\n",
		      ffa_error_code(ret));
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) == CACTUS_ERROR) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
