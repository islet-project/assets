/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <arch.h>
#include <arch_helpers.h>
#include <debug.h>
#include <test_helpers.h>
#include <tftf_lib.h>

typedef enum {
	EXEC_WFIT = 0,
	EXEC_WFET
} exec_wfxt;

#ifdef __aarch64__
static test_result_t test_wfxt_inst(exec_wfxt val, uint64_t ms)
{
	__asm__ volatile(".arch armv8.7-a");
	uint64_t timer_cnt1, timer_cnt2, feed_cnt;
	uint64_t timer_freq = read_cntfrq_el0();
	uint64_t ms_to_counts = ((ms * timer_freq) / 1000U);

	timer_cnt1 = virtualcounter_read();
	feed_cnt = timer_cnt1 + ms_to_counts;

	if (val == EXEC_WFIT) {
		wfit(feed_cnt);
	} else {
		wfet(feed_cnt);
	}

	timer_cnt2 = virtualcounter_read();

	/* Lapsed time should be at least equal to sleep time */
	if ((timer_cnt2 - timer_cnt1) >= ms_to_counts) {
		return TEST_RESULT_SUCCESS;
	} else {
		/* unlikely ends up here */
		uint64_t lapsed_ms = ((timer_cnt2 - timer_cnt1) * 1000) / timer_freq;

		ERROR("Time elapsed: actual(%llu)ms vs requested(%llu)ms \n",
		      lapsed_ms, ms);
		return TEST_RESULT_FAIL;
	}
}
#endif /* __aarch64__ */

test_result_t test_wfet_instruction(void)
{
	SKIP_TEST_IF_AARCH32();

#ifdef __aarch64__
	SKIP_TEST_IF_WFXT_NOT_SUPPORTED();

	/*
	 * first invocation of wfe returns immediately clearing the event
	 * register
	 */
	sevl();
	wfe();

	return test_wfxt_inst(EXEC_WFET, 10);
#endif /* __aarch64__ */
}

test_result_t test_wfit_instruction(void)
{
	test_result_t ret;

	SKIP_TEST_IF_AARCH32();

#ifdef __aarch64__
	SKIP_TEST_IF_WFXT_NOT_SUPPORTED();

	/* disable irqs to run wfi till timeout */
	disable_irq();

	ret = test_wfxt_inst(EXEC_WFIT, 10);

	/* enable irq back */
	enable_irq();
#endif /* __aarch64__ */

	return ret;
}
