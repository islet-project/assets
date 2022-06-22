/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>
#include <ffa_helpers.h>
#include <test_helpers.h>
#include <timer.h>

static volatile int timer_irq_received;

#define SENDER		HYP_ID
#define RECEIVER	SP_ID(1)
#define RECEIVER_2	SP_ID(2)
#define TIMER_DURATION	50U
#define SLEEP_TIME	100U

static const struct ffa_uuid expected_sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}
	};

/*
 * ISR for the timer interrupt. Update a global variable to check it has been
 * called.
 */
static int timer_handler(void *data)
{
	assert(timer_irq_received == 0);
	timer_irq_received = 1;
	return 0;
}

static int program_timer(unsigned long milli_secs)
{
	/* Program timer. */
	timer_irq_received = 0;
	tftf_timer_register_handler(timer_handler);

	return tftf_program_timer(milli_secs);
}

static int check_timer_interrupt(void)
{
	/* Check that the timer interrupt has been handled in NWd(TFTF). */
	tftf_cancel_timer();
	tftf_timer_unregister_handler();

	return timer_irq_received;
}

/*
 * @Test_Aim@ Test non-secure interrupts while a Secure Partition capable
 * of managed exit is executing.
 *
 * 1. Enable managed exit interrupt by sending interrupt_enable command to
 *    Cactus.
 *
 * 2. Register a handler for the non-secure timer interrupt. Program it to fire
 *    in a certain time.
 *
 * 3. Send a direct request request to Cactus SP to execute in busy loop.
 *
 * 4. While executing in busy loop, the non-secure timer should
 *    fire and trap into SPM running at S-EL2 as FIQ.
 *
 * 5. SPM injects a managed exit virtual FIQ into Cactus (as configured in the
 *    interrupt enable call), causing it to run its interrupt handler.
 *
 * 6. Cactus's managed exit handler acknowledges interrupt arrival by
 *    requesting the interrupt id to the SPMC, and check if it is the
 *    MANAGED_EXIT_INTERRUPT_ID.
 *
 * 7. Check whether the pending non-secure timer interrupt successfully got
 *    handled in TFTF.
 *
 * 8. Send a direct message request command to resume Cactus's execution.
 *    It resumes in the sleep loop and completes it. It then returns with
 *    a direct message response. Check if time lapsed is greater than
 *    sleeping time.
 *
 */
test_result_t test_ffa_ns_interrupt_managed_exit(void)
{
	int ret;
	struct ffa_value ret_values;

	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	/* Enable managed exit interrupt as FIQ in the secure side. */
	if (!spm_set_managed_exit_int(RECEIVER, true)) {
		return TEST_RESULT_FAIL;
	}

	ret = program_timer(TIMER_DURATION);
	if (ret < 0) {
		ERROR("Failed to program timer (%d)\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* Send request to primary Cactus to sleep for 100ms. */
	ret_values = cactus_sleep_cmd(SENDER, RECEIVER, SLEEP_TIME);

	if (!is_ffa_direct_response(ret_values)) {
		return TEST_RESULT_FAIL;
	}

	/*
	 * Managed exit interrupt occurs during this time, Cactus
	 * will respond with interrupt ID.
	 */
	if (cactus_get_response(ret_values) != MANAGED_EXIT_INTERRUPT_ID) {
		ERROR("Managed exit interrupt did not occur!\n");
		return TEST_RESULT_FAIL;
	}

	if (check_timer_interrupt() == 0) {
		ERROR("Timer interrupt hasn't actually been handled.\n");
		return TEST_RESULT_FAIL;
	}

	/*
	 * Send a dummy direct message request to relinquish CPU cycles.
	 * This resumes Cactus in the sleep routine.
	 */
	ret_values = ffa_msg_send_direct_req64(SENDER, RECEIVER,
					       0, 0, 0, 0, 0);

	if (!is_ffa_direct_response(ret_values)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret_values) == CACTUS_ERROR) {
		return TEST_RESULT_FAIL;
	}

	/* Make sure elapsed time not less than sleep time. */
	if (cactus_get_response(ret_values) < SLEEP_TIME) {
		ERROR("Lapsed time less than requested sleep time\n");
		return TEST_RESULT_FAIL;
	}

	/* Disable Managed exit interrupt. */
	if (!spm_set_managed_exit_int(RECEIVER, false)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Test the scenario where a non-secure interrupt triggers while a
 * Secure Partition,that specified action for NS interrupt as SIGNALABLE, is
 * executing.
 *
 * 1. Register a handler for the non-secure timer interrupt. Program it to fire
 *    in a certain time.
 *
 * 2. Send a direct request to Cactus SP to execute in busy loop.
 *
 * 3. While executing in busy loop, the non-secure timer should fire. Cactus SP
 *    should be preempted by non-secure interrupt.
 *
 * 4. Execution traps to SPMC running at S-EL2 as FIQ. SPMC returns control to
 *    the normal world through FFA_INTERRUPT ABI for it to handle the non-secure
 *    interrupt.
 *
 * 5. Check whether the pending non-secure timer interrupt successfully got
 *    handled in the normal world by TFTF.
 *
 * 6. Resume the Cactus SP using FFA_RUN ABI for it to complete the sleep
 *    routine.
 *
 * 7. Ensure the Cactus SP sends the DIRECT RESPONSE message.
 *
 * 8. Check if time lapsed is greater than sleep time.
 *
 */
test_result_t test_ffa_ns_interrupt_signaled(void)
{
	int ret;
	struct ffa_value ret_values;
	unsigned int core_pos = get_current_core_id();

	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	ret = program_timer(TIMER_DURATION);
	if (ret < 0) {
		ERROR("Failed to program timer (%d)\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* Send request to secondary Cactus to sleep for 100ms. */
	ret_values = cactus_sleep_cmd(SENDER, RECEIVER_2, SLEEP_TIME);

	if (check_timer_interrupt() == 0) {
		ERROR("Timer interrupt hasn't actually been handled.\n");
		return TEST_RESULT_FAIL;
	}

	/*
	 * Cactus SP should be preempted by non-secure interrupt. SPMC
	 * returns control to the normal world through FFA_INTERRUPT ABI
	 * for it to handle the non-secure interrupt.
	 */
	if (ffa_func_id(ret_values) != FFA_INTERRUPT) {
		ERROR("Expected FFA_INTERRUPT as return status!\n");
		return TEST_RESULT_FAIL;
	}

	/*
	 * Ensure SPMC returns FFA_ERROR with BUSY error code when a direct
	 * request message is sent to the preempted SP.
	 */
	ret_values = cactus_echo_send_cmd(SENDER, RECEIVER_2, ECHO_VAL1);

	if ((ffa_func_id(ret_values) != FFA_ERROR) ||
	    (ffa_error_code(ret_values) != FFA_ERROR_BUSY)) {
		ERROR("Expected FFA_ERROR(BUSY)! Got %x(%x)\n",
		      ffa_func_id(ret_values), ffa_error_code(ret_values));
		return TEST_RESULT_FAIL;
	}

	/*
	 * Resume the Cactus SP using FFA_RUN ABI for it to complete the
	 * sleep routine and send the direct response message.
	 */
	VERBOSE("Resuming %x\n", RECEIVER_2);
	ret_values = ffa_run(RECEIVER_2, core_pos);

	if (!is_ffa_direct_response(ret_values)) {
		return TEST_RESULT_FAIL;
	}

	/* Make sure elapsed time not less than sleep time. */
	if (cactus_get_response(ret_values) < SLEEP_TIME) {
		ERROR("Lapsed time less than requested sleep time\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
