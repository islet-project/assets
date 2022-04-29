/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/arm/arm_gic.h>
#include <mmio.h>
#include <platform.h>
#include <xlat_tables_v2.h>
#include <plat_arm.h>

/*
 * Table of regions to map using the MMU.
 */
static const mmap_region_t mmap[] = {
	MAP_REGION_FLAT(HOST_PERIPHERAL_BASE, HOST_PERIPHERAL_SIZE,
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(ON_CHIP_MEM_BASE, ON_CHIP_MEM_SIZE, MT_MEMORY | MT_RW | MT_SECURE),
#if USE_NVM
	MAP_REGION_FLAT(FLASH_BASE, FLASH_SIZE, MT_DEVICE | MT_RW | MT_NS),
#endif
	MAP_REGION_FLAT(DRAM_BASE, TFTF_BASE - DRAM_BASE, MT_MEMORY | MT_RW | MT_NS),
	{0}
};

const mmap_region_t *tftf_platform_get_mmap(void)
{
	return mmap;
}

void tftf_platform_setup(void)
{
	arm_platform_setup();
}

void plat_arm_gic_init(void)
{
	arm_gic_init(GICC_BASE, GICD_BASE, GICR_BASE);
}
