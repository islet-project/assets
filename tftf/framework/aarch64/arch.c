/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>

void tftf_arch_setup(void)
{
	/* Do not try to configure EL2 if TFTF is running at NS-EL1 */
	if (IS_IN_EL2()) {
		/* Enable asynchronous SError aborts to EL2 */
		enable_serror();

		/*
		 * Route physical interrupts to EL2 regardless of the value of
		 * the IMO/FMO bits. Without this, interrupts would not be taken
		 * and would remain pending, regardless of the PSTATE.{A, I, F}
		 * interrupt masks.
		 */
		write_hcr_el2(HCR_TGE_BIT);

		/*
		 * Disable trap of SVE instructions to EL2.
		 * The fields of the CPTR_EL2 register reset to an
		 * architecturally UNKNOWN value.
		 */
		write_cptr_el2(CPTR_EL2_RES1);
		isb();
	}
}
