/*
 * Copyright (c) 2018-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SGI_BASE_PLATFORM_DEF_H
#define SGI_BASE_PLATFORM_DEF_H

#include <lib/utils_def.h>

/* Platform binary types for linking */
#define PLATFORM_LINKER_FORMAT		"elf64-littleaarch64"
#define PLATFORM_LINKER_ARCH		aarch64

/* Sub-system Peripherals */
#define SGI_DEVICE0_BASE		UL(0x2A000000)
#define SGI_DEVICE0_SIZE		UL(0x26000000)

/* Peripherals and PCIe expansion area */
#define SGI_DEVICE1_BASE		UL(0x60000000)
#define SGI_DEVICE1_SIZE		UL(0x20000000)

/* AP Non-Secure UART related constants */
#define SGI_CSS_NSEC_UART_BASE		UL(0x2A400000)
#define SGI_CSS_NSEC_CLK_IN_HZ		7372800

#define PLAT_ARM_UART_BASE		SGI_CSS_NSEC_UART_BASE
#define PLAT_ARM_UART_CLK_IN_HZ		SGI_CSS_NSEC_CLK_IN_HZ

/* Base address of trusted watchdog (SP805) */
#define SP805_TWDOG_BASE		UL(0x2A480000)

/* Memory mapped Generic timer interfaces */
#define SYS_CNT_BASE1			UL(0x2A830000)

/* DRAM base address and size */
#define PLAT_ARM_DRAM1_BASE		UL(0x80000000)
#define PLAT_ARM_DRAM1_SIZE		UL(0x80000000)
#define DRAM_BASE			PLAT_ARM_DRAM1_BASE

/* TF-A reserves DRAM space 0xFF000000- 0xFFFFFFFF for TZC */
#define DRAM_SIZE			(PLAT_ARM_DRAM1_SIZE - 0x1000000)

/* Base address and size of external NVM flash */
#define FLASH_BASE			UL(0x08000000)
#define FLASH_SIZE			UL(0x04000000)  /* 64MB */
#define NOR_FLASH_BLOCK_SIZE		UL(0x40000)     /* 256KB */

/*******************************************************************************
 * Run-time address of the TFTF image.
 * It has to match the location where the Trusted Firmware-A loads the BL33
 * image.
 ******************************************************************************/
#define TFTF_BASE			UL(0xE0000000)

/*
 * If you want to use DRAM for non-volatile memory then the first 128MB
 * can be used. However for tests that involve power resets this is not
 * suitable since the state will be lost.
 */
#define TFTF_NVM_OFFSET			0x0
#define TFTF_NVM_SIZE			UL(0x08000000)	/* 128 MB */

/* Size of cacheable stacks */
#define PLATFORM_STACK_SIZE		0x1400

/* Size of coherent stacks */
#define PCPU_DV_MEM_STACK_SIZE		0x600

#define PLATFORM_CORE_COUNT		(PLAT_ARM_CLUSTER_COUNT * \
						CSS_SGI_MAX_CPUS_PER_CLUSTER)
#define PLATFORM_NUM_AFFS		(PLAT_ARM_CLUSTER_COUNT + PLATFORM_CORE_COUNT)
#define PLATFORM_MAX_AFFLVL		MPIDR_AFFLVL1

#define PLAT_MAX_PWR_LEVEL		PLATFORM_MAX_AFFLVL
#define PLAT_MAX_PWR_STATES_PER_LVL	2

/* Local state bit width for each level in the state-ID field of power state */
#define PLAT_LOCAL_PSTATE_WIDTH		4

/* Platform specific page table and MMU setup constants */
#define MAX_XLAT_TABLES			5
#define MAX_MMAP_REGIONS		16

/*******************************************************************************
 * Used to align variables on the biggest cache line size in the platform.
 * This is known only to the platform as it might have a combination of
 * integrated and external caches.
 ******************************************************************************/
#define CACHE_WRITEBACK_SHIFT		6
#define CACHE_WRITEBACK_GRANULE		(1 << CACHE_WRITEBACK_SHIFT)

/* Times(in ms) used by test code for completion of different events */
#define PLAT_SUSPEND_ENTRY_TIME		15
#define PLAT_SUSPEND_ENTRY_EXIT_TIME	30

/* I/O Storage NOR flash device */
#define MAX_IO_DEVICES			1
#define MAX_IO_HANDLES			1

/* Non-Secure Software Generated Interupts IDs */
#define IRQ_NS_SGI_0			0
#define IRQ_NS_SGI_7			7

/* Per-CPU Hypervisor Timer Interrupt ID */
#define IRQ_PCPU_HP_TIMER		26

#endif /* SGI_BASE_PLATFORM_DEF_H */
