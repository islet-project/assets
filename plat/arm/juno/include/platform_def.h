/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <utils_def.h>

#include "../juno_def.h"

/*******************************************************************************
 * Platform definitions used by common code
 ******************************************************************************/

#ifndef __PLATFORM_DEF_H__
#define __PLATFORM_DEF_H__

/*******************************************************************************
 * Platform binary types for linking
 ******************************************************************************/
#ifdef __aarch64__
#define PLATFORM_LINKER_FORMAT		"elf64-littleaarch64"
#define PLATFORM_LINKER_ARCH		aarch64
#else
#define PLATFORM_LINKER_FORMAT		"elf32-littlearm"
#define PLATFORM_LINKER_ARCH		arm
#endif

/*******************************************************************************
 * Run-time address of the TFTF image.
 * It has to match the location where the Trusted Firmware-A loads the BL33
 * image.
 ******************************************************************************/
#define TFTF_BASE			0xE0000000

#define JUNO_DRAM1_BASE			0x80000000
#define JUNO_DRAM2_BASE			0x880000000
#define DRAM_BASE			JUNO_DRAM1_BASE
#define DRAM_SIZE			0x80000000

/* Base address of non-trusted watchdog (SP805) */
#define SP805_WDOG_BASE			0x1C0F0000

/* Memory mapped Generic timer interfaces  */
#define SYS_CNT_BASE1		0x2a830000

/* V2M motherboard system registers & offsets */
#define VE_SYSREGS_BASE		0x1c010000
#define V2M_SYS_LED		0x8

/*******************************************************************************
 * Base address and size of external NVM flash
 ******************************************************************************/
#define FLASH_BASE			0x08000000

/*
 * The flash chip on Juno is a SCSP package of 2-die's and of a total size of
 * 512Mb, we are using only the main blocks of size 128KB for storing results.
 * The SMC controller performs data striping and splits the word into half to
 * each flash die's which leads to a virtual block size of 256KB to software.
 */
#define NOR_FLASH_BLOCK_SIZE		0x40000
#define NOR_FLASH_BLOCKS_COUNT		255
#define FLASH_SIZE			(NOR_FLASH_BLOCK_SIZE * NOR_FLASH_BLOCKS_COUNT)

/*******************************************************************************
 * Base address and size for the FIP that contains FWU images.
 ******************************************************************************/
#define PLAT_ARM_FWU_FIP_BASE		(FLASH_BASE + 0x400000)
#define PLAT_ARM_FWU_FIP_SIZE		(0x100000)

/*******************************************************************************
 * This is the temporary DDR address for loading backup fip.bin
 * image from NVM which is used for replacing original fip.bin
 * This address is chosen such that the NS_BL2U can be expanded
 * in future and also considering the large size of fip.bin.
 ******************************************************************************/
#define FIP_IMAGE_TMP_DDR_ADDRESS	(DRAM_BASE + 0x100000)

/*******************************************************************************
 * This offset is used to corrupt data in fip.bin
 * The offset is from the base where fip.bin is
 * located in NVM. This particular value is chosen
 * to make sure the corruption is done beyond fip header.
 ******************************************************************************/
#define FIP_CORRUPT_OFFSET		(0x400)

/*******************************************************************************
 * This offset is used to corrupt data in fip.bin
 * This is the base address for backup fip.bin image in NVM
 * which is used for replacing original fip.bin
 * This address is chosen such that it can stay with all
 * the other images in the NVM.
 ******************************************************************************/
#define FIP_BKP_ADDRESS			(FLASH_BASE + 0x1000000)

/*******************************************************************************
 * Base address and size for non-trusted SRAM.
 ******************************************************************************/
#define NSRAM_BASE				(0x2e000000)
#define NSRAM_SIZE				(0x00008000)

/*******************************************************************************
 * NS_BL1U specific defines.
 * NS_BL1U RW data is relocated from NS-ROM to NS-RAM at runtime so we
 * need 2 sets of addresses.
 ******************************************************************************/
#define NS_BL1U_BASE		(0x08000000 + 0x03EB8000)
#define NS_BL1U_RO_LIMIT	(NS_BL1U_BASE + 0xC000)

/*******************************************************************************
 * Put NS_BL1U RW at the top of the Non-Trusted SRAM. NS_BL1U_RW_BASE is
 * calculated using the current NS_BL1U RW debug size plus a little space
 * for growth.
 ******************************************************************************/
#define NS_BL1U_RW_SIZE		(0x7000)
#define NS_BL1U_RW_BASE		(NSRAM_BASE)
#define NS_BL1U_RW_LIMIT	(NS_BL1U_RW_BASE + NS_BL1U_RW_SIZE)

/*******************************************************************************
 * Base address and limit for NS_BL2U image.
 ******************************************************************************/
#define NS_BL2U_BASE		DRAM_BASE
#define NS_BL2U_LIMIT		(NS_BL2U_BASE + 0x4D000)

/*******************************************************************************
 * Generic platform constants
 ******************************************************************************/

/* Size of cacheable stacks */
#if IMAGE_NS_BL1U
#define PLATFORM_STACK_SIZE 0x1000
#elif IMAGE_NS_BL2U
#define PLATFORM_STACK_SIZE 0x1000
#elif IMAGE_TFTF
#define PLATFORM_STACK_SIZE 0x1400
#endif

/* Size of coherent stacks for debug and release builds */
#if DEBUG
#define PCPU_DV_MEM_STACK_SIZE 0x600
#else
#define PCPU_DV_MEM_STACK_SIZE 0x500
#endif

#define PLATFORM_SYSTEM_COUNT		1
#define PLATFORM_CLUSTER_COUNT		2
#define PLATFORM_CLUSTER1_CORE_COUNT	4 /* Cortex-A53 Cluster */
#define PLATFORM_CLUSTER0_CORE_COUNT	2 /* Cortex-A57 Cluster */
#define PLATFORM_CORE_COUNT		(PLATFORM_CLUSTER1_CORE_COUNT + \
					PLATFORM_CLUSTER0_CORE_COUNT)
#define PLATFORM_NUM_AFFS		(PLATFORM_SYSTEM_COUNT + \
					PLATFORM_CLUSTER_COUNT + \
					PLATFORM_CORE_COUNT)
#define PLATFORM_MAX_AFFLVL		MPIDR_AFFLVL2
#define PLAT_MAX_PWR_LEVEL		PLATFORM_MAX_AFFLVL
#define PLAT_MAX_PWR_STATES_PER_LVL	2

/* Local state bit width for each level in the state-ID field of power state */
#define PLAT_LOCAL_PSTATE_WIDTH		4

#if IMAGE_NS_BL1U
#define MAX_IO_DEVICES			2
#define MAX_IO_HANDLES			2
#else
#define MAX_IO_DEVICES			1
#define MAX_IO_HANDLES			1
#endif

#if USE_NVM
/*
 * The Flash memory is used to store TFTF data on Juno.
 * However, it might contain other data that must not be overwritten.
 * For example, when using the Trusted Firmware-A, the FIP image
 * (containing the bootloader images) is also stored in Flash.
 * Hence, consider the first 40MB of Flash as reserved for firmware usage.
 * The TFTF can use the rest of the Flash memory.
 */
#define TFTF_NVM_OFFSET		0x2800000	/* 40MB */
#define TFTF_NVM_SIZE		(FLASH_SIZE - TFTF_NVM_OFFSET)
#else
/*
 * If you want to run without support for non-volatile memory (due to e.g.
 * unavailability of a flash driver), DRAM can be used instead as workaround.
 * The TFTF binary itself is loaded at 0xE0000000 so we have plenty of free
 * memory at the beginning of the DRAM. Let's use the first 128MB.
 *
 * Please note that this won't be suitable for all test scenarios and
 * for this reason some tests will be disabled in this configuration.
 */
#define TFTF_NVM_OFFSET		0x0
#define TFTF_NVM_SIZE		0x8000000	/* 128 MB */
#endif

/*******************************************************************************
 * Platform specific page table and MMU setup constants
 ******************************************************************************/
#ifdef __aarch64__
#define PLAT_PHY_ADDR_SPACE_SIZE	(ULL(1) << 34)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(ULL(1) << 34)
#else
#define PLAT_PHY_ADDR_SPACE_SIZE	(ULL(1) << 32)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(ULL(1) << 32)
#endif

#if IMAGE_TFTF
/* For testing xlat tables lib v2 */
#define MAX_XLAT_TABLES			20
#define MAX_MMAP_REGIONS		50
#else
#define MAX_XLAT_TABLES			5
#define MAX_MMAP_REGIONS		16
#endif

/*******************************************************************************
 * Used to align variables on the biggest cache line size in the platform.
 * This is known only to the platform as it might have a combination of
 * integrated and external caches.
 ******************************************************************************/
#define CACHE_WRITEBACK_SHIFT   6
#define CACHE_WRITEBACK_GRANULE (1 << CACHE_WRITEBACK_SHIFT)

/*******************************************************************************
 * Non-Secure Software Generated Interupts IDs
 ******************************************************************************/
#define IRQ_NS_SGI_0		0
#define IRQ_NS_SGI_1		1
#define IRQ_NS_SGI_2		2
#define IRQ_NS_SGI_3		3
#define IRQ_NS_SGI_4		4
#define IRQ_NS_SGI_5		5
#define IRQ_NS_SGI_6		6
#define IRQ_NS_SGI_7		7

#define PLAT_MAX_SPI_OFFSET_ID	220

/* The IRQ generated by Ethernet controller */
#define IRQ_ETHERNET	192

#define IRQ_CNTPSIRQ1		92
/* Per-CPU Hypervisor Timer Interrupt ID */
#define IRQ_PCPU_HP_TIMER		26
/* Per-CPU Non-Secure Timer Interrupt ID */
#define IRQ_PCPU_NS_TIMER		30

/*
 * Times(in ms) used by test code for completion of different events.
 * Suspend entry time for debug build is high due to the time taken
 * by the VERBOSE/INFO prints. The value considers the worst case scenario
 * where all CPUs are going and coming out of suspend continuously.
 */
#if DEBUG
#define PLAT_SUSPEND_ENTRY_TIME		0x100
#define PLAT_SUSPEND_ENTRY_EXIT_TIME	0x200
#else
#define PLAT_SUSPEND_ENTRY_TIME		10
#define PLAT_SUSPEND_ENTRY_EXIT_TIME	20
#endif

#endif /* __PLATFORM_DEF_H__ */
