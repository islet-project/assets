/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <drivers/arm/pl011.h>
#include <drivers/console.h>
#include <io_storage.h>
#include <plat_arm.h>
#include <platform.h>
#include <platform_def.h>

#pragma weak tftf_platform_setup

void arm_platform_setup(void)
{
#if USE_NVM
	int ret;

	ret = arm_io_setup();
	if (ret != IO_SUCCESS)
		WARN("IO setup failed : 0x%x\n", ret);
#endif

#if IMAGE_NS_BL2U
	/* NS_BL2U is not expecting interrupts. */
	return;
#endif

	plat_arm_gic_init();

	arm_gic_setup_global();
	arm_gic_setup_local();
}

void tftf_platform_setup(void)
{
	arm_platform_setup();
}

void tftf_plat_arch_setup(void)
{
	tftf_plat_configure_mmu();
}

void tftf_early_platform_setup(void)
{
	console_init(PLAT_ARM_UART_BASE, PLAT_ARM_UART_CLK_IN_HZ,
		     PL011_BAUDRATE);
}
