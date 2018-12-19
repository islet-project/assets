/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <cactus_def.h>
#include <debug.h>
#include <smccc.h>
#include <spci_helpers.h>
#include <spci_svc.h>
#include <string.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <timer.h>

static volatile int timer_irq_received;

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

/*
 * @Test_Aim@ Test that non-secure interrupts interrupt non-blocking requests.
 *
 * 1. Register a handler for the non-secure timer interrupt. Program it to fire
 *    in a certain time.
 *
 * 2. Send a non-blocking request to Cactus to sleep for more time than the
 *    timer.
 *
 * 3. While servicing the timer sleep request, the non-secure timer should
 *    fire and interrupt Cactus.
 *
 * 4. Make sure that the response isn't ready yet.
 *
 * 5. In the TFTF, check that the interrupt has been handled.
 *
 * 6. Return to Cactus to finish the request.
 */
test_result_t test_spci_non_blocking_interrupt_by_ns(void)
{
	int ret;
	u_register_t ret1;
	uint16_t handle_cactus;
	uint32_t token_cactus;
	test_result_t result = TEST_RESULT_SUCCESS;

	SKIP_TEST_IF_SPCI_VERSION_LESS_THAN(0, 1);

	/* Open handle */

	ret = spci_service_handle_open(TFTF_SPCI_CLIENT_ID, &handle_cactus,
				       CACTUS_SERVICE1_UUID);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to return a valid handle. Returned: %d\n",
				     __LINE__, ret);
		return TEST_RESULT_FAIL;
	}

	/* Program timer */

	timer_irq_received = 0;
	tftf_timer_register_handler(timer_handler);

	ret = tftf_program_timer(100);
	if (ret < 0) {
		tftf_testcase_printf("Failed to program timer (%d)\n", ret);
		result = TEST_RESULT_FAIL;
	}

	/* Send request to Cactus */

	ret = spci_service_request_start(CACTUS_SLEEP_MS, 200U,
					 0, 0, 0, 0,
					 TFTF_SPCI_CLIENT_ID,
					 handle_cactus,
					 &token_cactus);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM should have returned SPCI_SUCCESS. Returned: %d\n",
				     __LINE__, ret);
		result = TEST_RESULT_FAIL;
	}

	/* Check that the interrupt has been handled */

	tftf_cancel_timer();
	tftf_timer_unregister_handler();

	if (timer_irq_received == 0) {
		tftf_testcase_printf("%d: Didn't handle interrupt\n", __LINE__);
		result = TEST_RESULT_FAIL;
	}

	/* Make sure that the response is not ready yet */

	ret = spci_service_get_response(TFTF_SPCI_CLIENT_ID,
					handle_cactus,
					token_cactus,
					NULL, NULL, NULL);

	if (ret == SPCI_SUCCESS) {
		tftf_testcase_printf("%d: Cactus returned SPCI_SUCCESS\n",
				__LINE__);
		result = TEST_RESULT_FAIL;
		goto exit_close_handle;
	}

	/* Re-enter Cactus to finish the request */

	do {
		ret = spci_service_request_resume(TFTF_SPCI_CLIENT_ID,
						  handle_cactus,
						  token_cactus,
						  &ret1, NULL, NULL);
	} while (ret == SPCI_QUEUED);

	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: Cactus returned 0x%x\n",
				__LINE__, (uint32_t)ret);
		result = TEST_RESULT_FAIL;
	}

	/* Close handle */
exit_close_handle:

	ret = spci_service_handle_close(TFTF_SPCI_CLIENT_ID, handle_cactus);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to close the handle. Returned: %d\n",
				     __LINE__, ret);
		result = TEST_RESULT_FAIL;
	}

	/* All tests finished */

	return result;
}
