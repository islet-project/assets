/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <mm_svc.h>
#include <secure_partition.h>
#include <smccc.h>
#include <spm_svc.h>
#include <string.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <timer.h>
#include <xlat_tables_v2.h>

static volatile int timer_irq_received;

/*
 * ISR for the timer interrupt.
 * Just update a global variable to prove it has been called.
 */
static int timer_handler(void *data)
{
	assert(timer_irq_received == 0);
	timer_irq_received = 1;
	return 0;
}


/*
 * @Test_Aim@ Test that non-secure interrupts do not interrupt secure service
 * requests.
 *
 * 1. Register a handler for the non-secure timer interrupt.
 *
 * 2. Program the non-secure timer to fire in 500 ms.
 *
 * 3. Make a long-running (> 500 ms) fast secure service request.
 *    This is achieved by requesting the timer sleep service in Cactus
 *    with a 1 second sleep delay.
 *
 * 4. While servicing the timer sleep request, the non-secure timer should
 *    fire but not interrupt Cactus.
 *
 * 5. Once back in TFTF, check the response from Cactus, which shows whether the
 *    secure service indeed ran to completion.
 *
 * 6. Also check whether the pending non-secure timer interrupt successfully got
 *    handled in TFTF.
 */
test_result_t test_secure_partition_interrupt_by_ns(void)
{
	secure_partition_request_info_t *sps_request;
	test_result_t result = TEST_RESULT_FAIL;

	SKIP_TEST_IF_MM_VERSION_LESS_THAN(1, 0);

	VERBOSE("Mapping NS<->SP shared buffer\n");

	int rc = mmap_add_dynamic_region(ARM_SECURE_SERVICE_BUFFER_BASE,
					 ARM_SECURE_SERVICE_BUFFER_BASE,
					 ARM_SECURE_SERVICE_BUFFER_SIZE,
					 MT_MEMORY | MT_RW | MT_NS);
	if (rc != 0) {
		tftf_testcase_printf("%d: mmap_add_dynamic_region() = %d\n",
				     __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	timer_irq_received = 0;
	tftf_timer_register_handler(timer_handler);

	NOTICE("Programming the timer...\n");
	rc = tftf_program_timer(500);
	if (rc < 0) {
		tftf_testcase_printf("Failed to program timer (%d)\n", rc);
		goto exit_test;
	}

	INFO("Sending MM_COMMUNICATE_AARCH64 to Cactus\n");

	uint8_t timer_delay = 1;
	sps_request = create_sps_request(SPS_TIMER_SLEEP,
					 &timer_delay, sizeof(timer_delay));
	smc_args mm_communicate_smc = {
		MM_COMMUNICATE_AARCH64,
		0, /* cookie, MBZ */
		(uintptr_t) sps_request,
		0
	};

	smc_ret_values smc_ret = tftf_smc(&mm_communicate_smc);

	INFO("Returned from Cactus, MM_COMMUNICATE_AARCH64 handling complete\n");

	/*
	 * If MM_COMMUNICATE gets interrupted, SPM will return SPM_QUEUED, which
	 * is normally not a valid return value for MM_COMMUNICATE.
	 */
	if ((uint32_t) smc_ret.ret0 != SPM_SUCCESS) {
		tftf_testcase_printf("Cactus returned: 0x%x\n",
				     (uint32_t) smc_ret.ret0);
		goto exit_test;
	}

	uint32_t cactus_response;
	memcpy(&cactus_response, sps_request->data, sizeof(cactus_response));
	if (cactus_response != CACTUS_FAST_REQUEST_SUCCESS) {
		tftf_testcase_printf("Error code from the timer secure service: 0x%x\n",
				     cactus_response);
		goto exit_test;
	}

	/*
	 * If the timer interrupt is still pending, make sure it is taken right
	 * now.
	 */
	isb();

	if (timer_irq_received == 1)
		result = TEST_RESULT_SUCCESS;

exit_test:
	tftf_cancel_timer();
	tftf_timer_unregister_handler();

	VERBOSE("Unmapping NS<->SP shared buffer\n");

	mmap_remove_dynamic_region(ARM_SECURE_SERVICE_BUFFER_BASE,
				   ARM_SECURE_SERVICE_BUFFER_SIZE);

	return result;
}
