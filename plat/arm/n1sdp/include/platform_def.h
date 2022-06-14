/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <arch.h>

#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

/* Platform binary types for linking */
#define PLATFORM_LINKER_FORMAT		"elf64-littleaarch64"
#define PLATFORM_LINKER_ARCH		aarch64

#define N1SDP_CLUSTER_COUNT		2
#define N1SDP_MAX_CPUS_PER_CLUSTER	2
#define N1SDP_MAX_PE_PER_CPU		1

/*******************************************************************************
 * Run-time address of the TFTF image.
 * It has to match the location where the Trusted Firmware-A loads the BL33
 * image.
 ******************************************************************************/
#define TFTF_BASE			0xE0000000

#define N1SDP_DRAM1_BASE		0x80000000
#define N1SDP_DRAM1_SIZE		0x80000000
#define DRAM_BASE			N1SDP_DRAM1_BASE

/*
 * TF-A reserves DRAM space 0xFD000000 - 0xFEFFFFFF for Trusted DRAM
 * TF-A reserves DRAM space 0xFF000000 - 0xFFFFFFFF for TZC
 */
#define ARM_TZC_DRAM1_SIZE		0x00200000 /* 2MB */
#define ARM_TRUSTED_DRAM1_SIZE		0x0E000000 /* 16MB */

#define DRAM_SIZE			(N1SDP_DRAM1_SIZE	-	\
					 ARM_TRUSTED_DRAM1_SIZE -	\
					 ARM_TZC_DRAM1_SIZE)

/* REFCLK CNTControl, Generic Timer. Secure Access only. */
#define SYS_CNT_CONTROL_BASE		0x2a430000
/* REFCLK CNTRead, Generic Timer. */
#define SYS_CNT_READ_BASE		0x2a800000
/* AP_REFCLK CNTBase1, Generic Timer. */
#define SYS_CNT_BASE1			0x2A830000

/* Base address of non-trusted watchdog (SP805) */
#define SP805_WDOG_BASE			0x1C0F0000

/* Base address of trusted watchdog (SP805) */
#define SP805_TWDOG_BASE		0x2A480000
#define IRQ_TWDOG_INTID			86

/* Base address and size of external NVM flash */
#define FLASH_BASE			0x08000000

#define NOR_FLASH_BLOCK_SIZE		0x40000		/* 256KB */
#define FLASH_SIZE			0x4000000	/* 64MB */

/*
 * If you want to use DRAM for non-volatile memory then the first 128MB
 * can be used. However for tests that involve power resets this is not
 * suitable since the state will be lost.
 */
#define TFTF_NVM_OFFSET			0x0
#define TFTF_NVM_SIZE			0x8000000	/* 128 MB */

/* Sub-system Peripherals */
#define N1SDP_DEVICE0_BASE		0x08000000
#define N1SDP_DEVICE0_SIZE		0x48000000

/* N1SDP remote chip at 4 TB offset */
#define PLAT_ARM_REMOTE_CHIP_OFFSET             (ULL(1) << 42)

/* Following covers remote n1sdp */
#define N1SDP_DEVICE1_BASE		(N1SDP_DEVICE0_BASE + PLAT_ARM_REMOTE_CHIP_OFFSET)
#define N1SDP_DEVICE1_SIZE		N1SDP_DEVICE0_SIZE

/* GIC-600 & interrupt handling related constants */
#define N1SDP_GICD_BASE			0x30000000
#define N1SDP_GICR_BASE			0x300C0000
#define N1SDP_GICC_BASE			0x2C000000

/* SoC's PL011 UART0 related constants */
#define PL011_UART0_BASE		0x2A400000
#define PL011_UART0_CLK_IN_HZ		50000000

/*
 * SoC's PL011 UART1 related constants (duplicated from UART0 since AP UART1
 * isn't accessible on N1SDP)
 */
#define PL011_UART1_BASE		0x2A400000
#define PL011_UART1_CLK_IN_HZ		50000000

#define PLAT_ARM_UART_BASE		PL011_UART0_BASE
#define PLAT_ARM_UART_CLK_IN_HZ		PL011_UART0_CLK_IN_HZ

/* Size of cacheable stacks */
#define PLATFORM_STACK_SIZE		0x1400

/* Size of coherent stacks */
#define PCPU_DV_MEM_STACK_SIZE		0x600

#define PLATFORM_CORE_COUNT		(N1SDP_CLUSTER_COUNT * N1SDP_MAX_CPUS_PER_CLUSTER)
#define PLATFORM_NUM_AFFS		(N1SDP_CLUSTER_COUNT + PLATFORM_CORE_COUNT)
#define PLATFORM_MAX_AFFLVL		MPIDR_AFFLVL1

#define PLAT_MAX_PWR_LEVEL		PLATFORM_MAX_AFFLVL
#define PLAT_MAX_PWR_STATES_PER_LVL	2

/* I/O Storage NOR flash device */
#define MAX_IO_DEVICES			1
#define MAX_IO_HANDLES			1

/* Local state bit width for each level in the state-ID field of power state */
#define PLAT_LOCAL_PSTATE_WIDTH		4

/* Platform specific page table and MMU setup constants */
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ull << 36)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ull << 36)

#if IMAGE_CACTUS
#define MAX_XLAT_TABLES			6
#else
#define MAX_XLAT_TABLES			5
#endif
#define MAX_MMAP_REGIONS		16

/*******************************************************************************
 * Used to align variables on the biggest cache line size in the platform.
 * This is known only to the platform as it might have a combination of
 * integrated and external caches.
 ******************************************************************************/
#define CACHE_WRITEBACK_SHIFT  		 6
#define CACHE_WRITEBACK_GRANULE		 (1 << CACHE_WRITEBACK_SHIFT)

/* Non-Secure Software Generated Interupts IDs */
#define IRQ_NS_SGI_0			0
#define IRQ_NS_SGI_7			7

/*
 * AP UART1 interrupt is considered as the maximum SPI.
 * MAX_SPI_ID = MIN_SPI_ID + PLAT_MAX_SPI_OFFSET_ID = 96
 */
#define PLAT_MAX_SPI_OFFSET_ID		64

/* AP_REFCLK Generic Timer, Non-secure. */
#define IRQ_CNTPSIRQ1			92

/* Per-CPU Hypervisor Timer Interrupt ID */
#define IRQ_PCPU_HP_TIMER		26

/* Times(in ms) used by test code for completion of different events */
#define PLAT_SUSPEND_ENTRY_TIME         0x100
#define PLAT_SUSPEND_ENTRY_EXIT_TIME    0x200

#endif /* PLATFORM_DEF_H */
