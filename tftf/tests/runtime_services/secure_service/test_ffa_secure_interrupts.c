/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>
#include <ffa_helpers.h>
#include <test_helpers.h>
#include <timer.h>

#define SENDER		HYP_ID
#define RECEIVER	SP_ID(1)
#define SP_SLEEP_TIME	1000U
#define NS_TIME_SLEEP	1500U
#define ECHO_VAL1	U(0xa0a0a0a0)

static const struct ffa_uuid expected_sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}
	};

static bool configure_trusted_wdog_interrupt(ffa_id_t source, ffa_id_t dest,
				bool enable)
{
	smc_ret_values ret_values;

	ret_values = cactus_interrupt_cmd(source, dest, IRQ_TWDOG_INTID,
					  enable, INTERRUPT_TYPE_IRQ);

	if (!is_ffa_direct_response(ret_values)) {
		ERROR("Expected a direct response message while configuring"
			" TWDOG interrupt\n");
		return false;
	}

	if (cactus_get_response(ret_values) != CACTUS_SUCCESS) {
		ERROR("Failed to configure Trusted Watchdog interrupt\n");
		return false;
	}
	return true;
}

static bool enable_trusted_wdog_interrupt(ffa_id_t source, ffa_id_t dest)
{
	return configure_trusted_wdog_interrupt(source, dest, true);
}

static bool disable_trusted_wdog_interrupt(ffa_id_t source, ffa_id_t dest)
{
	return configure_trusted_wdog_interrupt(source, dest, false);
}

/*
 * @Test_Aim@ Test secure interrupt handling while first Secure Partition is
 * in RUNNING state.
 *
 * 1. Send a direct message request command to first Cactus SP to start the
 *    trusted watchdog timer.
 *
 * 2. Send a command to SP to sleep by executing a busy loop.
 *
 * 3. While SP is running the busy loop, Secure interrupt should trigger during
 *    this time.
 *
 * 4. The interrupt will be trapped to SPM as IRQ. SPM will inject the virtual
 *    IRQ to the first SP through vIRQ conduit and perform eret to resume
 *    execution in SP.
 *
 * 5. Execution traps to irq handler of Cactus SP. It will handle the secure
 *    interrupt triggered by the trusted watchdog timer.
 *
 * 6. Cactus SP will perform End-Of-Interrupt and resume execution in the busy
 *    loop.
 *
 * 7. Cactus SP will send a direct response message with the elapsed time back
 *    to the normal world.
 *
 * 8. We make sure the time elapsed in the sleep routine by SP is not less than
 *    the requested value.
 *
 * 9. For robustness of state transition checks, TFTF sends echo command using
 *    a direct request message.
 *
 * 10. Further, TFTF expects SP to return with a success value through a direct
 *    response message.
 *
 * 11. Test finishes successfully once the TFTF disables the trusted watchdog
 *     interrupt through a direct message request command.
 *
 */
test_result_t test_ffa_sec_interrupt_sp_running(void)
{
	smc_ret_values ret_values;

	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	/* Enable trusted watchdog interrupt as IRQ in the secure side. */
	if (!enable_trusted_wdog_interrupt(SENDER, RECEIVER)) {
		return TEST_RESULT_FAIL;
	}

	ret_values = cactus_send_twdog_cmd(SENDER, RECEIVER, 50);

	if (!is_ffa_direct_response(ret_values)) {
		ERROR("Expected a direct response for starting TWDOG timer\n");
		return TEST_RESULT_FAIL;
	}

	/* Send request to first Cactus SP to sleep */
	ret_values = cactus_sleep_cmd(SENDER, RECEIVER, SP_SLEEP_TIME);

	/*
	 * Secure interrupt should trigger during this time, Cactus
	 * will handle the trusted watchdog timer.
	 */
	if (!is_ffa_direct_response(ret_values)) {
		ERROR("Expected a direct response for sleep command\n");
		return TEST_RESULT_FAIL;
	}

	VERBOSE("Secure interrupt has preempted execution: %u\n",
					cactus_get_response(ret_values));

	/* Make sure elapsed time not less than sleep time */
	if (cactus_get_response(ret_values) < SP_SLEEP_TIME) {
		ERROR("Lapsed time less than requested sleep time\n");
		return TEST_RESULT_FAIL;
	}

	ret_values = cactus_echo_send_cmd(SENDER, RECEIVER, ECHO_VAL1);

	if (!is_ffa_direct_response(ret_values)) {
		ERROR("Expected direct response for echo command\n");
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret_values) != CACTUS_SUCCESS ||
	    cactus_echo_get_val(ret_values) != ECHO_VAL1) {
		ERROR("Echo Failed!\n");
		return TEST_RESULT_FAIL;
	}

	/* Disable Trusted Watchdog interrupt. */
	if (!disable_trusted_wdog_interrupt(SENDER, RECEIVER)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
