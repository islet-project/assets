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

	write_hcr(HCR_TGE_BIT);
}
