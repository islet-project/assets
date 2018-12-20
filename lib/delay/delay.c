/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>

void waitus(uint64_t us)
{
	uint64_t start_count_val = syscounter_read();
	uint64_t wait_cycles = (us * read_cntfrq_el0()) / 1000000;

	while ((syscounter_read() - start_count_val) < wait_cycles)
		/* Busy wait... */;
}

void waitms(uint64_t ms)
{
	while (ms > 0) {
		waitus(1000);
		ms--;
	}
}
