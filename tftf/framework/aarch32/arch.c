/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <tftf.h>


void tftf_arch_setup(void)
{
	if (!IS_IN_HYP())
		panic();

	/*
	 * Route physical interrupts to Hyp mode regardless of the value of the
	 * IMO/FMO bits. Without this, interrupts would not be taken and would
	 * remain pending, regardless of the PSTATE.{A, I, F} interrupt masks.
	 */
	write_hcr(HCR_TGE_BIT);
}
