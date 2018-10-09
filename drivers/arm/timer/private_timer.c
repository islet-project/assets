/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <platform.h>

/*******************************************************************************
 * Data structure to keep track of per-cpu secure generic timer context across
 * power management operations.
 ******************************************************************************/
typedef struct timer_context {
	uint64_t cval;
	uint32_t ctl;
} timer_context_t;

static timer_context_t pcpu_timer_context[PLATFORM_CORE_COUNT];

/*******************************************************************************
 * This function initializes the generic timer to fire every `timeo` ms.
 ******************************************************************************/
void private_timer_start(unsigned long timeo)
{
	uint64_t cval, freq;
	uint32_t ctl = 0;

	/* Any previous pending timer activation will be disabled. */
	cval = read_cntpct_el0();
	freq = read_cntfrq_el0();
	cval += (freq * timeo) / 1000;
	write_cnthp_cval_el2(cval);

	/* Enable the secure physical timer */
	set_cntp_ctl_enable(ctl);
	write_cnthp_ctl_el2(ctl);
}

/*******************************************************************************
 * This function deasserts the timer interrupt prior to cpu power down
 ******************************************************************************/
void private_timer_stop(void)
{
	/* Disable the timer */
	write_cnthp_ctl_el2(0);
}

/*******************************************************************************
 * This function saves the timer context prior to cpu suspension
 ******************************************************************************/
void private_timer_save(void)
{
	uint32_t linear_id = platform_get_core_pos(read_mpidr_el1());

	pcpu_timer_context[linear_id].cval = read_cnthp_cval_el2();
	pcpu_timer_context[linear_id].ctl = read_cnthp_ctl_el2();
	flush_dcache_range((uintptr_t) &pcpu_timer_context[linear_id],
			   sizeof(pcpu_timer_context[linear_id]));
}

/*******************************************************************************
 * This function restores the timer context post cpu resummption
 ******************************************************************************/
void private_timer_restore(void)
{
	uint32_t linear_id = platform_get_core_pos(read_mpidr_el1());

	write_cnthp_cval_el2(pcpu_timer_context[linear_id].cval);
	write_cnthp_ctl_el2(pcpu_timer_context[linear_id].ctl);
}
