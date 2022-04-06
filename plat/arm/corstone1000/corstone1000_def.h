/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __CORSTONE1000_DEF_H__
#define __CORSTONE1000_DEF_H__

#include <common_def.h>
#include <platform_def.h>

/*******************************************************************************
 * HOST memory map related constants
 ******************************************************************************/

#define HOST_PERIPHERAL_BASE		(0x1A000000)
#define HOST_PERIPHERAL_SIZE		(608 * SZ_1M)

#define ON_CHIP_MEM_BASE		(0x02000000)
#define ON_CHIP_MEM_SIZE		(SZ_4M)

#define DRAM_BASE			(0x80000000)
#define DRAM_SIZE			(SZ_2G)
/*******************************************************************************
 * GIC-400 & interrupt handling related constants
 ******************************************************************************/
/* GIC memory map */
#define GICD_BASE		0x1C010000
#define GICC_BASE		0x1C02F000
/* GIC re-distributor doesn't exits on gic-400, but we still need to
 * provide GICR_BASE as the gic driver needs it
 */
#define GICR_BASE		0x0

/*******************************************************************************
 * PL011 related constants
 ******************************************************************************/
#define PL011_UART0_BASE	0x1A510000
#define PL011_UART1_BASE	0x1A520000

#define PL011_UART0_CLK_IN_HZ	50000000
#define PL011_UART1_CLK_IN_HZ	50000000

#define PLAT_ARM_UART_BASE		PL011_UART0_BASE
#define PLAT_ARM_UART_CLK_IN_HZ		PL011_UART0_CLK_IN_HZ

#endif /* __CORSTONE1000_DEF_H__ */
