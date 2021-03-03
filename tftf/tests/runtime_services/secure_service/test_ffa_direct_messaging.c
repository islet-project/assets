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
#include <lib/events.h>
#include <lib/power_management.h>
#include <platform.h>
#include <test_helpers.h>

#define ECHO_VAL1 U(0xa0a0a0a0)
#define ECHO_VAL2 U(0xb0b0b0b0)
#define ECHO_VAL3 U(0xc0c0c0c0)

static const struct ffa_uuid expected_sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}
	};


static event_t cpu_booted[PLATFORM_CORE_COUNT];

static test_result_t send_cactus_echo_cmd(ffa_id_t sender,
					  ffa_id_t dest,
					  uint64_t value)
{
	smc_ret_values ret;
	ret = cactus_echo_send_cmd(sender, dest, value);

	/*
	 * Return responses may be FFA_MSG_SEND_DIRECT_RESP or FFA_INTERRUPT,
	 * but only expect the former. Expect SMC32 convention from SP.
	 */
	if (!is_ffa_direct_response(ret)) {
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
static test_result_t send_cactus_req_echo_cmd(ffa_id_t sender,
					      ffa_id_t dest,
					      ffa_id_t echo_dest,
					      uint64_t value)
{
	smc_ret_values ret;

	ret = cactus_req_echo_send_cmd(sender, dest, echo_dest, value);

	if (!is_ffa_direct_response(ret)) {
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

	if (is_ffa_direct_response(ret) == false) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) == CACTUS_ERROR) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * Handler that is passed during tftf_cpu_on to individual CPU cores.
 * Runs a specific core and send a direct message request.
 * Expects core_pos | SP_ID as a response.
 */
static test_result_t cpu_on_handler(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);
	test_result_t ret = TEST_RESULT_SUCCESS;
	smc_ret_values ffa_ret;

	/*
	 * Send a direct message request to SP1 (MP SP) from current physical
	 * CPU. Notice SP1 ECs are already woken as a result of the PSCI_CPU_ON
	 * invocation so they already reached the message loop.
	 * The SPMC uses the MP pinned context corresponding to the physical
	 * CPU emitting the request.
	 */
	ret = send_cactus_echo_cmd(HYP_ID, SP_ID(1), ECHO_VAL1);
	if (ret != TEST_RESULT_SUCCESS) {
		goto out;
	}

	/*
	 * Secure Partitions beyond the first SP only have their first
	 * EC (or vCPU0) woken up at boot time by the SPMC.
	 * Other ECs need one round of ffa_run to reach the message loop.
	 */
	ffa_ret = ffa_run(SP_ID(2), core_pos);
	if (ffa_func_id(ffa_ret) != FFA_MSG_WAIT) {
		ERROR("Failed to run SP%x on core %u\n", SP_ID(2),
				core_pos);
		ret = TEST_RESULT_FAIL;
		goto out;
	}

	/*
	 * Send a direct message request to SP2 (MP SP) from current physical
	 * CPU. The SPMC uses the MP pinned context corresponding to the
	 * physical CPU emitting the request.
	 */
	ret = send_cactus_echo_cmd(HYP_ID, SP_ID(2), ECHO_VAL2);
	if (ret != TEST_RESULT_SUCCESS) {
		goto out;
	}

	/*
	 * Send a direct message request to SP3 (UP SP) from current physical CPU.
	 * The SPMC uses the single vCPU migrated to the new physical core.
	 * The single SP vCPU may receive requests from multiple physical CPUs.
	 * Thus it is possible one message is being processed on one core while
	 * another (or multiple) cores attempt sending a new direct message
	 * request. In such case the cores attempting the new request receive
	 * a busy response from the SPMC. To handle this case a retry loop is
	 * implemented permitting some fairness.
	 */
	uint32_t trial_loop = 5U;
	while (trial_loop--) {
		ffa_ret = cactus_echo_send_cmd(HYP_ID, SP_ID(3), ECHO_VAL3);
		if ((ffa_func_id(ffa_ret) == FFA_ERROR) &&
		    (ffa_error_code(ffa_ret) == FFA_ERROR_BUSY)) {
			VERBOSE("%s(%u) trial %u\n", __func__, core_pos, trial_loop);
			waitms(1);
			continue;
		}

		if (is_ffa_direct_response(ffa_ret) == true) {
			if (cactus_get_response(ffa_ret) != CACTUS_SUCCESS ||
				cactus_echo_get_val(ffa_ret) != ECHO_VAL3) {
				ERROR("Echo Failed!\n");
				ret = TEST_RESULT_FAIL;
			}

			goto out;
		}
	}

	ret = TEST_RESULT_FAIL;

out:
	/* Tell the lead CPU that the calling CPU has completed the test */
	tftf_send_event(&cpu_booted[core_pos]);

	return ret;
}

/**
 * Test direct messaging in multicore setup. Runs SPs on all the cores and sends
 * direct messages to SPs.
 */
test_result_t test_ffa_secondary_core_direct_msg(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos, cpu_node, mpidr;
	int32_t ret;

	/**********************************************************************
	 * Check SPMC has ffa_version and expected FFA endpoints are deployed.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	for (unsigned int i = 0U; i < PLATFORM_CORE_COUNT; i++) {
		tftf_init_event(&cpu_booted[i]);
	}

	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == lead_mpid) {
			continue;
		}

		ret = tftf_cpu_on(mpidr, (uintptr_t)cpu_on_handler, 0U);
		if (ret != 0) {
			ERROR("tftf_cpu_on mpidr 0x%x returns %d\n", mpidr, ret);
		}
	}

	VERBOSE("Waiting secondary CPUs to turn off ...\n");

	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(mpidr);
		tftf_wait_for_event(&cpu_booted[core_pos]);
	}

	VERBOSE("Done exiting.\n");

	/**********************************************************************
	 * All tests passed.
	 **********************************************************************/

	return TEST_RESULT_SUCCESS;
}
