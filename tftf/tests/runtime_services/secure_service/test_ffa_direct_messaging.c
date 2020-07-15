/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <cactus_def.h>
#include <platform.h>
#include <smccc.h>
#include <ffa_helpers.h>
#include <ffa_svc.h>
#include <test_helpers.h>

#define DIRECT_MSG_TEST_PATTERN1	(0xaaaa0000)
#define DIRECT_MSG_TEST_PATTERN2	(0xbbbb0000)
#define DIRECT_MSG_TEST_PATTERN3	(0xcccc0000)

#define OPTEE_FFA_GET_API_VERSION	(0)
#define OPTEE_FFA_GET_OS_VERSION	(1)
#define OPTEE_FFA_GET_OS_VERSION_MAJOR	(3)
#define OPTEE_FFA_GET_OS_VERSION_MINOR	(8)

static test_result_t send_receive_direct_msg(unsigned int sp_id,
					     unsigned int test_pattern)
{
	smc_ret_values ret_values;

	/* Send a message to SP through direct messaging */
	ret_values = ffa_msg_send_direct_req(HYP_ID, SP_ID(sp_id),
					      test_pattern);

	/*
	 * Return responses may be FFA_MSG_SEND_DIRECT_RESP or FFA_INTERRUPT,
	 * but only expect the former. Expect SMC32 convention from SP.
	 */
	if (ret_values.ret0 != FFA_MSG_SEND_DIRECT_RESP_SMC32) {
		tftf_testcase_printf("ffa_msg_send_direct_req returned %lx\n",
				     (u_register_t)ret_values.ret0);
		return TEST_RESULT_FAIL;
	}

	/*
	 * Message loop in SP returns initial message with the running VM id
	 * into the lower 16 bits of initial message.
	 */
	if (ret_values.ret3 != (test_pattern | sp_id)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * check_spmc_execution_level
 *
 * Attempt sending impdef protocol messages to OP-TEE through direct messaging.
 * Criteria for detecting OP-TEE presence is that responses match defined
 * version values. In the case of SPMC running at S-EL2 (and Cactus instances
 * running at S-EL1) the response will not match the pre-defined version IDs.
 *
 * Returns true if SPMC is probed as being OP-TEE at S-EL1.
 *
 */
static bool check_spmc_execution_level(void)
{
	unsigned int is_optee_spmc_criteria = 0;
	smc_ret_values ret_values;

	/*
	 * Send a first OP-TEE-defined protocol message through
	 * FFA direct message.
	 *
	 */
	ret_values = ffa_msg_send_direct_req(HYP_ID, SP_ID(1),
					      OPTEE_FFA_GET_API_VERSION);
	if ((ret_values.ret3 == FFA_VERSION_MAJOR) &&
	    (ret_values.ret4 == FFA_VERSION_MINOR)) {
		is_optee_spmc_criteria++;
	}

	/*
	 * Send a second OP-TEE-defined protocol message through
	 * FFA direct message.
	 *
	 */
	ret_values = ffa_msg_send_direct_req(HYP_ID, SP_ID(1),
					      OPTEE_FFA_GET_OS_VERSION);
	if ((ret_values.ret3 == OPTEE_FFA_GET_OS_VERSION_MAJOR) &&
	    (ret_values.ret4 == OPTEE_FFA_GET_OS_VERSION_MINOR)) {
		is_optee_spmc_criteria++;
	}

	return (is_optee_spmc_criteria == 2);
}

test_result_t test_ffa_direct_messaging(void)
{
	test_result_t result;

	/**********************************************************************
	 * Verify that FFA is there and that it has the correct version.
	 **********************************************************************/
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 0);

	/* Check if SPMC is OP-TEE at S-EL1 */
	if (check_spmc_execution_level()) {
		/* Direct messaging is successfully verified for OP-TEE */
		return TEST_RESULT_SUCCESS;
	}

	/*
	 * From this point assume SPMC runs at S-EL2 and two Cactus instances
	 * are loaded at S-EL1.
	 *
	 */

	/**********************************************************************
	 * Send a message to SP1 through direct messaging
	 **********************************************************************/
	result = send_receive_direct_msg(1, DIRECT_MSG_TEST_PATTERN1);
	if (result != TEST_RESULT_SUCCESS) {
		return result;
	}

	/**********************************************************************
	 * Send a message to SP2 through direct messaging
	 **********************************************************************/
	result = send_receive_direct_msg(2, DIRECT_MSG_TEST_PATTERN2);
	if (result != TEST_RESULT_SUCCESS) {
		return result;
	}

	/**********************************************************************
	 * Send a message to SP1 through direct messaging
	 **********************************************************************/
	result = send_receive_direct_msg(1, DIRECT_MSG_TEST_PATTERN3);
	if (result != TEST_RESULT_SUCCESS) {
		return result;
	}

	/**********************************************************************
	 * All tests passed.
	 **********************************************************************/
	return TEST_RESULT_SUCCESS;
}
