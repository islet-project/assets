/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <tftf.h>

void waitus(uint64_t us)
{
	uint64_t cntp_ct_val_base;
	uint32_t cnt_frq;
	uint64_t wait_cycles;

	cnt_frq = read_cntfrq_el0();
	cntp_ct_val_base = read_cntpct_el0();

	/* Waitms in terms of counter freq */
	wait_cycles = (us * cnt_frq) / 1000000;

	while (read_cntpct_el0() - cntp_ct_val_base < wait_cycles)
		;
}

void waitms(uint64_t ms)
{
	while (ms > 0) {
		waitus(1000);
		ms--;
	}
}
