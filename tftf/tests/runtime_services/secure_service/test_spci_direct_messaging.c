/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <cactus_def.h>
#include <platform.h>
#include <smccc.h>
#include <spci_helpers.h>
#include <spci_svc.h>
#include <test_helpers.h>

/* Hypervisor ID at physical SPCI instance */
#define HYP_ID		(0)

/* By convention, SP IDs (as opposed to VM IDs) have bit 15 set */
#define SP_ID(x)	(x | (1 << 15))

#define DIRECT_MSG_TEST_PATTERN1	(0xaaaa0000)
#define DIRECT_MSG_TEST_PATTERN2	(0xbbbb0000)
#define DIRECT_MSG_TEST_PATTERN3	(0xcccc0000)

static test_result_t send_receive_direct_msg(unsigned int sp_id,
					     unsigned int test_pattern)
{
	smc_ret_values ret_values;

	/* Send a message to SP through direct messaging */
	ret_values = spci_msg_send_direct_req(HYP_ID, SP_ID(sp_id),
					      test_pattern);

	/*
	 * Return responses may be SPCI_MSG_SEND_DIRECT_RESP or SPCI_INTERRUPT,
	 * but only expect the former. Expect SMC32 convention from SP.
	 */
	if (ret_values.ret0 != SPCI_MSG_SEND_DIRECT_RESP_SMC32) {
		tftf_testcase_printf("spci_msg_send_direct_req returned %lx\n",
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

test_result_t test_spci_direct_messaging(void)
{
	smc_ret_values ret_values;
	test_result_t result;

	/**********************************************************************
	 * Verify that SPCI is there and that it has the correct version.
	 **********************************************************************/
	SKIP_TEST_IF_SPCI_VERSION_LESS_THAN(0, 9);

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
	/*
	 * NOTICE: for now, the SPM does not initially run each SP sequentially
	 * on boot up so we explicitely run the SP once by invoking SPCI_RUN so
	 * it reaches spci_msg_wait in the message loop function.
	 */

	/* Request running SP2 on VCPU0 */
	ret_values = spci_run(2, 0);
	if (ret_values.ret0 != SPCI_MSG_WAIT) {
		tftf_testcase_printf("spci_run returned %lx\n",
				     (u_register_t)ret_values.ret0);
		return TEST_RESULT_FAIL;
	}

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
