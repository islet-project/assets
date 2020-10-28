/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>

#include <psci.h>
#include <tftf.h>

void tftf_plat_reset(void)
{
	smc_args reset_args = { SMC_PSCI_SYSTEM_RESET };

	(void)tftf_smc(&reset_args);
	bug_unreachable();
}
