/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>

void tftf_arch_setup(void)
{
	/* Do not try to configure EL2 if TFTF is running at NS-EL1 */
	if (IS_IN_EL2()) {
		write_hcr_el2(HCR_TGE_BIT);
		isb();
	}
}
