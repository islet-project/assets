/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

#include <arch.h>

#define PLATFORM_LINKER_FORMAT		"elf64-littleaarch64"
#define PLATFORM_LINKER_ARCH		aarch64

#define TFTF_BASE			0x1AC98000


#define CACHE_WRITEBACK_GRANULE		0x40


#define PLATFORM_CLUSTER_COUNT		2
#define PLATFORM_CORE_COUNT_PER_CLUSTER	4
#define PLATFORM_CORE_COUNT		(PLATFORM_CLUSTER_COUNT * \
					PLATFORM_CORE_COUNT_PER_CLUSTER)
#define PLATFORM_NUM_AFFS		(PLATFORM_CORE_COUNT + \
						PLATFORM_CLUSTER_COUNT + 1)
#define PLATFORM_MAX_AFFLVL		MPIDR_AFFLVL2
#define PLAT_MAX_PWR_LEVEL		MPIDR_AFFLVL2
#define PLAT_MAX_PWR_STATES_PER_LVL	2


#define PLATFORM_STACK_SIZE		0x2000
#define PCPU_DV_MEM_STACK_SIZE		0x100


#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ULL << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ULL << 32)
#define MAX_XLAT_TABLES			3
#define MAX_MMAP_REGIONS		16

#define DRAM_BASE			0x0
#define DRAM_SIZE			0xE0000000

#define FLASH_BASE			0x0
#define FLASH_SIZE			0xC0000000

/*
 * TFTF_NVM_OFFSET/SIZE correspond to the NVM partition in the partition table
 */
#define TFTF_NVM_SIZE			0x600000
#define TFTF_NVM_OFFSET			0xBFA00000

/* Local state bit width for each level in the state-ID field of power state */
#define PLAT_LOCAL_PSTATE_WIDTH		4

/* GIC-400 related addresses from datasheet */
#define GICD_REG_BASE			0xE82B1000
#define GICC_REG_BASE			0xE82B2000

/*******************************************************************************
 * Non-Secure Software Generated Interupts IDs
 ******************************************************************************/
#define IRQ_NS_SGI_0			0
#define IRQ_NS_SGI_1			1
#define IRQ_NS_SGI_2			2
#define IRQ_NS_SGI_3			3
#define IRQ_NS_SGI_4			4
#define IRQ_NS_SGI_5			5
#define IRQ_NS_SGI_6			6
#define IRQ_NS_SGI_7			7

/* Per-CPU Hypervisor Timer Interrupt ID */
#define IRQ_PCPU_HP_TIMER		26
/* Datasheet: TIME00 event*/
#define IRQ_CNTPSIRQ1			80

#define PLAT_MAX_SPI_OFFSET_ID		343
#define SYS_CNT_BASE1			0xfff14000
#define SP805_WDOG_BASE			0xE8A06000

/* ARM PL011 UART */
#define CRASH_CONSOLE_BASE		0xFFF32000
#define PL011_BAUDRATE			115200
#define PL011_UART_CLK_IN_HZ		19200000

/*
 * Times(in ms) used by test code for completion of different events. Kept the
 * same as in FVP.
 */
#define PLAT_SUSPEND_ENTRY_TIME		15
#define PLAT_SUSPEND_ENTRY_EXIT_TIME	30

/*
 * Dummy definitions that we need just to compile...
 */
#define ARM_SECURE_SERVICE_BUFFER_BASE	0
#define ARM_SECURE_SERVICE_BUFFER_SIZE	100

#endif /* PLATFORM_DEF_H */
