/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __JUNO_DEF_H__
#define __JUNO_DEF_H__

#include <platform_def.h>

/*******************************************************************************
 * Juno memory map related constants
 ******************************************************************************/
/* First peripherals region excluding Non-Trusted ROM and Non-Trusted RAM */
#define DEVICE0_BASE		0x20000000
#define DEVICE0_SIZE		0x0e000000

/* PCIe expansion region and 2nd peripherals region */
#define DEVICE1_BASE		0x40000000
#define DEVICE1_SIZE		0x40000000

#define IOFPGA_PERIPHERALS_BASE	0x1c000000
#define IOFPGA_PERIPHERALS_SIZE	0x3000000

/*******************************************************************************
 * GIC-400 & interrupt handling related constants
 ******************************************************************************/
#define GICD_BASE		0x2c010000
#define GICC_BASE		0x2c02f000
/* Juno doesn't support GIC Redistributor, it's a GICv3 feature */
#define GICR_BASE		0

/*******************************************************************************
 * PL011 related constants
 ******************************************************************************/
/* SoC UART0 */
#define PL011_UART2_BASE		0x7ff80000
#define PL011_UART2_CLK_IN_HZ		7273800

#define PLAT_ARM_UART_BASE		PL011_UART2_BASE
#define PLAT_ARM_UART_CLK_IN_HZ		PL011_UART2_CLK_IN_HZ

/*******************************************************************************
 * Motherboard timer related constants
 ******************************************************************************/
#define MB_TIMER1_BASE		0x1C120000
#define MB_TIMER1_IRQ		198
#define MB_TIMER1_FREQ		32000

/*******************************************************************************
 * Ethernet controller related constants
 ******************************************************************************/
#define ETHERNET_BASE			0x18000000
#define ETHERNET_SIZE			0x04000000
#define ETHERNET_IRQ_CFG_OFFSET		0x54
#define ETHERNET_IRQ_CFG_VAL		0x11

#endif /* __JUNO_DEF_H__ */

