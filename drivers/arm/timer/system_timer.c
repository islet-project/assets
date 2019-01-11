/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <drivers/arm/arm_gic.h>
#include <drivers/arm/gic_v2.h>
#include <drivers/arm/system_timer.h>
#include <debug.h>
#include <irq.h>
#include <mmio.h>

static uintptr_t g_systimer_base;

int program_systimer(unsigned long time_out_ms)
{
	unsigned int cntp_ctl;
	unsigned long long count_val;
	unsigned int freq;

	/* Check timer base is initialised */
	assert(g_systimer_base);

	count_val = mmio_read_64(g_systimer_base + CNTPCT_LO);
	freq = read_cntfrq_el0();
	count_val += (freq * time_out_ms) / 1000;
	mmio_write_64(g_systimer_base + CNTP_CVAL_LO, count_val);

	/* Enable the timer */
	cntp_ctl = mmio_read_32(g_systimer_base + CNTP_CTL);
	set_cntp_ctl_enable(cntp_ctl);
	clr_cntp_ctl_imask(cntp_ctl);
	mmio_write_32(g_systimer_base + CNTP_CTL, cntp_ctl);

	/*
	 * Ensure that we have programmed a timer interrupt for a time in
	 * future. Else we will have to wait for the systimer to rollover
	 * for the interrupt to fire (which is 64 years).
	 */
	if (count_val < mmio_read_64(g_systimer_base + CNTPCT_LO))
		panic();

	VERBOSE("%s : interrupt requested at sys_counter: %llu "
		"time_out_ms: %ld\n", __func__, count_val, time_out_ms);

	return 0;
}

static void disable_systimer(void)
{
	uint32_t val;

	/* Check timer base is initialised */
	assert(g_systimer_base);

	/* Deassert and disable the timer interrupt */
	val = 0;
	set_cntp_ctl_imask(val);
	mmio_write_32(g_systimer_base + CNTP_CTL, val);
}

int cancel_systimer(void)
{
	disable_systimer();
	return 0;
}

int handler_systimer(void)
{
	disable_systimer();
	return 0;
}

int init_systimer(uintptr_t systimer_base)
{
	/* Check timer base is not initialised */
	assert(!g_systimer_base);

	g_systimer_base = systimer_base;

	/* Disable the timer as the reset value is unknown */
	disable_systimer();

	/* Initialise CVAL to zero */
	mmio_write_64(g_systimer_base + CNTP_CVAL_LO, 0);

	return 0;
}
