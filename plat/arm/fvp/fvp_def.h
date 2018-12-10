/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * FVP specific definitions. Used only by FVP specific code.
 ******************************************************************************/

#ifndef __FVP_DEF_H__
#define __FVP_DEF_H__

#include <platform_def.h>

/*******************************************************************************
 * Cluster Topology definitions
 ******************************************************************************/
#define FVP_MAX_CPUS_PER_CLUSTER	4
/* Currently the highest cluster count on the FVP is 4 (Quad cluster) */
#define FVP_CLUSTER_COUNT		4
/* Currently multi-threaded CPUs only have a single thread */
#define FVP_MAX_PE_PER_CPU		1

/*******************************************************************************
 * FVP memory map related constants
 ******************************************************************************/

#define DEVICE0_BASE		0x1a000000
#define DEVICE0_SIZE		0x12200000

#define DEVICE1_BASE		0x2f000000
#define DEVICE1_SIZE		0x400000

/*******************************************************************************
 * GIC-400 & interrupt handling related constants
 ******************************************************************************/
/* Base FVP compatible GIC memory map */
#define GICD_BASE		0x2f000000
#define GICR_BASE		0x2f100000
#define GICC_BASE		0x2c000000

/*******************************************************************************
 * PL011 related constants
 ******************************************************************************/
#define PL011_UART0_BASE	0x1c090000
#define PL011_UART1_BASE	0x1c0a0000
#define PL011_UART2_BASE	0x1c0b0000
#define PL011_UART3_BASE	0x1c0c0000

#define PL011_UART0_CLK_IN_HZ	24000000
#define PL011_UART1_CLK_IN_HZ	24000000
#define PL011_UART2_CLK_IN_HZ	24000000
#define PL011_UART3_CLK_IN_HZ	24000000

#define PLAT_ARM_UART_BASE		PL011_UART0_BASE
#define PLAT_ARM_UART_CLK_IN_HZ		PL011_UART0_CLK_IN_HZ

#endif /* __FVP_DEF_H__ */
