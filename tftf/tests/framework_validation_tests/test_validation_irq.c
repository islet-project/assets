/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <drivers/arm/arm_gic.h>
#include <irq.h>
#include <platform.h>
#include <sgi.h>
#include <tftf_lib.h>

static volatile unsigned int counter;

/*
 * IRQ handler for SGI #0.
 * Increment the test counter to prove it's been successfully called.
 */
static int increment_counter(void *data)
{
	counter++;
	return 0;
}

#if !DEBUG
static int set_counter_to_42(void *data)
{
	counter = 42;
	return 0;
}
#endif

/*
 * @Test_Aim@ Test IRQ handling on lead CPU
 *
 * Check that IRQ enabling/disabling and IRQ handler registering/unregistering
 * work as expected on the lead CPU.
 */
test_result_t test_validation_irq(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);
	const unsigned int sgi_id = IRQ_NS_SGI_0;
	int ret;

	counter = 0;

	/* Now register a handler */
	ret = tftf_irq_register_handler(sgi_id, increment_counter);
	if (ret != 0) {
		tftf_testcase_printf("Failed to register initial IRQ handler\n");
		return TEST_RESULT_FAIL;
	}

	tftf_irq_enable(sgi_id, GIC_HIGHEST_NS_PRIORITY);

	/*
	 * Send the SGI to the calling core and check the IRQ handler has been
	 * successfully called
	 */
	tftf_send_sgi(sgi_id, core_pos);

	/* Wait till the handler is executed */
	while (counter != 1)
		;

	/*
	 * Try to overwrite the IRQ handler. This should fail.
	 * In debug builds, it would trigger an assertion so we can't test that
	 * as it will stop the test session.
	 * In release builds though, it should just do nothing, i.e. it won't
	 * replace the existing handler and that's something that can be tested.
	 */
#if !DEBUG
	ret = tftf_irq_register_handler(sgi_id, set_counter_to_42);
	if (ret == 0) {
		tftf_testcase_printf(
			"Overwriting the IRQ handler should have failed\n");
		return TEST_RESULT_FAIL;
	}
#endif

	tftf_send_sgi(sgi_id, core_pos);
	while (counter != 2)
		;

	/* Unregister the IRQ handler */
	ret = tftf_irq_unregister_handler(sgi_id);
	if (ret != 0) {
		tftf_testcase_printf("Failed to unregister IRQ handler\n");
		return TEST_RESULT_FAIL;
	}

	/*
	 * Send the SGI to the calling core and check the former IRQ handler
	 * has not been called, now that it has been unregistered.
	 */
	tftf_send_sgi(sgi_id, core_pos);

	/*
	 * Wait for some time so that SGI interrupts the processor, Normally it
	 * takes a small but finite time for the IRQ to be sent to processor
	 */
	waitms(500);

	if (counter != 2) {
		tftf_testcase_printf(
			"IRQ handler hasn't been successfully unregistered\n");
		return TEST_RESULT_FAIL;
	}

	/*
	 * Try to unregister the IRQ handler again. This should fail.
	 * In debug builds, it would trigger an assertion so we can't test that
	 * as it will stop the test session.
	 * In release builds though, it should just do nothing.
	 */
#if !DEBUG
	ret = tftf_irq_unregister_handler(sgi_id);
	if (ret == 0) {
		tftf_testcase_printf(
			"Unregistering the IRQ handler again should have failed\n");
		return TEST_RESULT_FAIL;
	}
#endif

	tftf_irq_disable(sgi_id);

	return TEST_RESULT_SUCCESS;
}
