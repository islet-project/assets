/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/arm/arm_gic.h>
#include <mmio.h>
#include <plat_arm.h>
#include <platform.h>
#include <xlat_tables_v2.h>

/*
 * Table of regions to map using the MMU.
 */
#if IMAGE_NS_BL1U
static const mmap_region_t mmap[] = {
	MAP_REGION_FLAT(DEVICE1_BASE, DEVICE1_SIZE, MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(FLASH_BASE, FLASH_SIZE, MT_MEMORY | MT_RO | MT_NS),
	MAP_REGION_FLAT(DRAM_BASE, DRAM_SIZE, MT_MEMORY | MT_RW | MT_NS),
	{0}
};
#elif IMAGE_NS_BL2U
static const mmap_region_t mmap[] = {
	MAP_REGION_FLAT(DEVICE0_BASE, DEVICE0_SIZE, MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(IOFPGA_PERIPHERALS_BASE, IOFPGA_PERIPHERALS_SIZE,
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(DEVICE1_BASE, DEVICE1_SIZE, MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(FLASH_BASE, FLASH_SIZE, MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(DRAM_BASE, DRAM_SIZE, MT_MEMORY | MT_RW | MT_NS),
	{0}
};
#elif IMAGE_TFTF
static const mmap_region_t mmap[] = {
	MAP_REGION_FLAT(DEVICE0_BASE, DEVICE0_SIZE, MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(ETHERNET_BASE, ETHERNET_SIZE, MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(IOFPGA_PERIPHERALS_BASE, IOFPGA_PERIPHERALS_SIZE,
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(DEVICE1_BASE, DEVICE1_SIZE, MT_DEVICE | MT_RW | MT_NS),
#if USE_NVM
	MAP_REGION_FLAT(FLASH_BASE, FLASH_SIZE, MT_DEVICE | MT_RW | MT_NS),
#endif
	MAP_REGION_FLAT(DRAM_BASE, TFTF_BASE - DRAM_BASE, MT_MEMORY | MT_RW | MT_NS),
	{0}
};
#endif	/* IMAGE_NS_BL1U */

const mmap_region_t *tftf_platform_get_mmap(void)
{
	return mmap;
}

void tftf_platform_setup(void)
{
	arm_platform_setup();

#if !IMAGE_NS_BL2U
	/*
	 * The Ethernet IRQ line is high by default which prevents Juno
	 * from entering system suspend. Configure it to be low.
	 *
	 * Interrupts are disabled in NS_BL2U so there's no need to fix this
	 * as we are not going to suspend the system.
	 *
	 * TODO: Currently this needs to be done in a loop for the write
	 * to IRQ_CFG register to take effect. Need to find the reason for
	 * this behavior.
	 */
	int val;

	do {
		mmio_write_32(ETHERNET_BASE + ETHERNET_IRQ_CFG_OFFSET,
				ETHERNET_IRQ_CFG_VAL);

		val = mmio_read_8(ETHERNET_BASE + ETHERNET_IRQ_CFG_OFFSET);
	} while (val != ETHERNET_IRQ_CFG_VAL);
#endif /* IMAGE_NS_BL2U */
}

void plat_arm_gic_init(void)
{
	arm_gic_init(GICC_BASE, GICD_BASE, GICR_BASE);
}
