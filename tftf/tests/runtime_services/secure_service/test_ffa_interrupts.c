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
#define RECEIVER_3	SP_ID(3)
#define TIMER_DURATION	50U
#define SLEEP_TIME	100U
#define SLEEP_TIME_FWD	200U

static const struct ffa_uuid expected_sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}
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

/*
 * @Test_Aim@ This test exercises the following scenario: Managed exit is
 * supported by both SPs in a call chain. A non-secure interrupt triggers
 * while the second SP is processing a direct request message sent by the first
 * SP. We choose SP(1) as the first SP and SP(3) as the second SP.
 *
 * 1. Enable managed exit interrupt by sending interrupt_enable command to both
 *    the Cactus SPs.
 *
 * 2. Register a handler for the non-secure timer interrupt. Program it to fire
 *    in a certain time.
 *
 * 3. Send a direct request to the first SP(i.e., SP(1)) to forward sleep command to
 *    the second SP(i.e., SP(3)).
 *
 * 4. While the second SP is running the busy loop, non-secure interrupt would
 *    trigger during this time.
 *
 * 5. The interrupt will be trapped to SPMC as FIQ. SPMC will inject the managed
 *    exit signal to the second SP through vIRQ conduit and perform eret to
 *    resume execution in the second SP.
 *
 * 6. The second SP sends the managed exit direct response to the first SP
 *    through its interrupt handler for managed exit.
 *
 * 7. SPMC proactively injects managed exit signal to the first SP through vFIQ
 *    conduit and resumes it using eret.
 *
 * 8. The first Cactus SP sends the managed exit direct response to TFTF through
 *    its interrupt handler for managed exit.
 *
 * 9. TFTF checks the return value in the direct message response from the first SP
 *    and ensures it is managed signal interrupt ID.
 *
 * 10. Check whether the pending non-secure timer interrupt successfully got
 *     handled in the normal world by TFTF.
 *
 * 11. Send a dummy direct message request command to resume the first SP's execution.
 *
 * 12. The first SP direct message request returns with managed exit response. It
 *     then sends a dummy direct message request command to resume the second SP's
 *     execution.
 *
 * 13. The second SP resumes in the sleep routine and sends a direct message
 *     response to the first SP.
 *
 * 14. The first SP checks if time lapsed is not lesser than sleep time and if
 *     successful, sends direct message response to the TFTF.
 *
 * 15. TFTF ensures the direct message response did not return with an error.
 *
 * 16. TFTF further disables the managed exit virtual interrupt for both the
 *     Cactus SPs.
 *
 */
test_result_t test_ffa_ns_interrupt_managed_exit_chained(void)
{
	int ret;
	struct ffa_value ret_values;

	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	/* Enable managed exit interrupt in the secure side. */
	if (!spm_set_managed_exit_int(RECEIVER, true) ||
		!spm_set_managed_exit_int(RECEIVER_3, true)) {
		return TEST_RESULT_FAIL;
	}

	ret = program_timer(TIMER_DURATION);
	if (ret < 0) {
		ERROR("Failed to program timer (%d)\n", ret);
		return TEST_RESULT_FAIL;
	}

	/*
	 * Send request to first Cactus SP to send request to another Cactus
	 * SP to sleep.
	 */
	ret_values = cactus_fwd_sleep_cmd(SENDER, RECEIVER, RECEIVER_3,
					  SLEEP_TIME_FWD);

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

	/* Disable Managed exit interrupt. */
	if (!spm_set_managed_exit_int(RECEIVER, false) ||
		!spm_set_managed_exit_int(RECEIVER_3, false)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ This test exercises the following scenario: Managed exit is
 * supported by the first SP but not by the second SP in a call chain. A
 * non-secure interrupt triggers while the second SP is processing a direct request
 * message sent by the first SP. We choose SP(1) as the first SP and SP(2) as
 * the second SP.
 *
 * 1. Enable managed exit interrupt by sending interrupt_enable command to
 *    the first Cactus SP in the call chain.
 *
 * 2. Register a handler for the non-secure timer interrupt. Program it to fire
 *    in a certain time.
 *
 * 3. Send a direct request to the first SP(i.e., SP(1)) to forward sleep command to
 *    the second SP(i.e., SP(2)).
 *
 * 4. While the second SP is running the busy loop, non-secure interrupt would
 *    trigger during this time.
 *
 * 5. The interrupt will be trapped to SPMC as FIQ. SPMC finds the source of
 *    the interrupted direct message request and prepares the return status
 *    as FFA_INTERRUPT.
 *
 * 6. SPMC injects managed exit signal to the first SP through vFIQ
 *    conduit and resumes it using eret.
 *
 * 7. The first Cactus SP sends the managed exit direct response to TFTF through
 *    its interrupt handler for managed exit.
 *
 * 8. TFTF checks the return value in the direct message response from the first SP
 *    and ensures it is managed signal interrupt ID.
 *
 * 9. Check whether the pending non-secure timer interrupt successfully got
 *    handled in the normal world by TFTF.
 *
 * 10. Send a dummy direct message request command to resume the first SP's execution.
 *
 * 11. The first SP direct message request returns with FFA_INTERRUPT status. It
 *     then resumes the second SP's execution using FFA_RUN ABI.
 *
 * 12. The second SP resumes in the sleep routine and sends a direct message
 *     response to the first SP.
 *
 * 13. The first SP checks if time lapsed is not lesser than sleep time and if
 *     successful, sends direct message response to the TFTF.
 *
 * 14. TFTF ensures the direct message response did not return with an error.
 *
 * 15. TFTF further disables the managed exit virtual interrupt for the first
 *     Cactus SP.
 *
 */
test_result_t test_ffa_SPx_ME_SPy_signaled(void)
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

	/*
	 * Send request to first Cactus SP to send request to another Cactus
	 * SP to sleep.
	 */
	ret_values = cactus_fwd_sleep_cmd(SENDER, RECEIVER, RECEIVER_2,
					  SLEEP_TIME_FWD);

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

	/* Disable Managed exit interrupt. */
	if (!spm_set_managed_exit_int(RECEIVER, false)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Test the scenario where a non-secure interrupt triggers while a
 * Secure Partition,that specified action for NS interrupt as QUEUED, is
 * executing.
 *
 * 1. Register a handler for the non-secure timer interrupt. Program it to fire
 *    in a certain time.
 *
 * 2. Send a direct request request to Cactus SP to execute in busy loop.
 *
 * 3. While executing in busy loop, the non-secure timer should fire. Cactus SP
 *    should be NOT be preempted by non-secure interrupt.
 *
 * 4. Cactus SP should complete the sleep routine and return with a direct
 *    response message.
 *
 * 5. Ensure that elapsed time in the sleep routine is not less than sleep time
 *    requested through direct message request.
 *
 */
test_result_t test_ffa_ns_interrupt_queued(void)
{
	int ret;
	struct ffa_value ret_values;

	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	ret = program_timer(TIMER_DURATION);
	if (ret < 0) {
		ERROR("Failed to program timer (%d)\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* Send request to a Cactus SP to sleep for 100ms. */
	ret_values = cactus_sleep_cmd(SENDER, RECEIVER_3, SLEEP_TIME);

	if (check_timer_interrupt() == 0) {
		ERROR("Timer interrupt hasn't actually been handled.\n");
		return TEST_RESULT_FAIL;
	}

	/*
	 * Cactus SP should not be preempted by non-secure interrupt. It
	 * should complete the sleep routine and return with a direct response
	 * message.
	 */
	if (!is_ffa_direct_response(ret_values)) {
		ERROR("Expected direct message response\n");
		return TEST_RESULT_FAIL;
	}

	/* Make sure elapsed time not less than sleep time. */
	if (cactus_get_response(ret_values) < SLEEP_TIME) {
		ERROR("Lapsed time less than requested sleep time\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
