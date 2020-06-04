/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <drivers/console.h>
#include <drivers/arm/gic_common.h>
#include <drivers/arm/gic_v2.h>
#include <platform.h>
#include <platform_def.h>

#include <xlat_tables_v2.h>

/*
 * Memory map
 */
static const mmap_region_t tegra194_mmap[] = {
	MAP_REGION_FLAT(TEGRA_MC_BASE, 0x2000, /* 8KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_TMR0_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_WDT0_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_GICD_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_GICC_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_UARTC_BASE, 0x20000U, /* 128KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_RTC_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_TMRUS_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_AOWAKE_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_SCRATCH_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_SMMU0_BASE, 0x1000, /* 4KB */
			MT_DEVICE | MT_RW | MT_NS),
	MAP_REGION_FLAT(DRAM_BASE + TFTF_NVM_OFFSET, TFTF_NVM_SIZE,
			MT_MEMORY | MT_RW | MT_NS),
	MAP_REGION_FLAT(TEGRA_SMMU_CTX_BASE, 0x1000, /* 4KB */
			MT_MEMORY | MT_RW | MT_NS),
	{0}
};

const mmap_region_t *tftf_platform_get_mmap(void)
{
	return tegra194_mmap;
}

void tftf_plat_arch_setup(void)
{
	tftf_plat_configure_mmu();
}

void tftf_early_platform_setup(void)
{
	/* Tegra194 platforms use UARTC as the console */
	console_init(TEGRA_UARTC_BASE, TEGRA_CONSOLE_CLKRATE,
			TEGRA_CONSOLE_BAUDRATE);
}

void tftf_platform_setup(void)
{
	gicv2_init(TEGRA_GICC_BASE, TEGRA_GICD_BASE);
	gicv2_setup_distif();
	gicv2_probe_gic_cpu_id();
	gicv2_setup_cpuif();

	/* Setup power management dependencies */
	tegra_pwr_mgmt_setup();

	/* Configure system suspend wake sources */
	tegra_set_rtc_as_wakeup_source();
}
