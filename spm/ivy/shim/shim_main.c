/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <drivers/arm/pl011.h>
#include <drivers/console.h>
#include <errno.h>
#include <ffa_helpers.h>
#include <lib/aarch64/arch_helpers.h>
#include <lib/xlat_tables/xlat_mmu_helpers.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat_arm.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <sp_debug.h>
#include <sp_helpers.h>
#include <std_svc.h>

#include "ivy.h"
#include "ivy_def.h"

static void shim_print_memory_layout(void)
{
	NOTICE("Secure Partition memory layout:\n");

	NOTICE("  Image regions\n");
	NOTICE("    Text region            : %p - %p\n",
		(void *)IVY_TEXT_START, (void *)IVY_TEXT_END);
	NOTICE("    Read-only data region  : %p - %p\n",
		(void *)IVY_RODATA_START, (void *)IVY_RODATA_END);
	NOTICE("    Data region            : %p - %p\n",
		(void *)IVY_DATA_START, (void *)IVY_DATA_END);
	NOTICE("    BSS region             : %p - %p\n",
		(void *)IVY_BSS_START, (void *)IVY_BSS_END);
	NOTICE("    Total image memory     : %p - %p\n",
		(void *)IVY_IMAGE_BASE,
		(void *)(IVY_IMAGE_BASE + IVY_IMAGE_SIZE));
	NOTICE("  SPM regions\n");
	NOTICE("    SPM <-> SP buffer      : %p - %p\n",
		(void *)IVY_SPM_BUF_BASE,
		(void *)(IVY_SPM_BUF_BASE + IVY_SPM_BUF_SIZE));
	NOTICE("    NS <-> SP buffer       : %p - %p\n",
		(void *)IVY_NS_BUF_BASE,
		(void *)(IVY_NS_BUF_BASE + IVY_NS_BUF_SIZE));
}

static void shim_plat_configure_mmu(void)
{
	mmap_add_region(SHIM_TEXT_START,
			SHIM_TEXT_START,
			SHIM_TEXT_END - SHIM_TEXT_START,
			MT_CODE | MT_PRIVILEGED);
	mmap_add_region(SHIM_RODATA_START,
			SHIM_RODATA_START,
			SHIM_RODATA_END - SHIM_RODATA_START,
			MT_RO_DATA | MT_PRIVILEGED);
	mmap_add_region(SHIM_DATA_START,
			SHIM_DATA_START,
			SHIM_DATA_END - SHIM_DATA_START,
			MT_RW_DATA | MT_PRIVILEGED);
	mmap_add_region(SHIM_BSS_START,
			SHIM_BSS_START,
			SHIM_BSS_END - SHIM_BSS_START,
			MT_RW_DATA | MT_PRIVILEGED);
	mmap_add_region(IVY_TEXT_START,
			IVY_TEXT_START,
			IVY_TEXT_END - IVY_TEXT_START,
			MT_CODE | MT_USER);
	mmap_add_region(IVY_RODATA_START,
			IVY_RODATA_START,
			IVY_RODATA_END - IVY_RODATA_START,
			MT_RO_DATA | MT_USER);
	mmap_add_region(IVY_DATA_START,
			IVY_DATA_START,
			IVY_DATA_END - IVY_DATA_START,
			MT_RW_DATA | MT_USER);
	mmap_add_region(IVY_BSS_START,
			IVY_BSS_START,
			IVY_BSS_END - IVY_BSS_START,
			MT_RW_DATA | MT_USER);

	init_xlat_tables();
}

int shim_main(void)
{
	assert(IS_IN_EL1() != 0);

	/* Initialise console */
	set_putc_impl(HVC_CALL_AS_STDOUT);

	NOTICE("Booting S-EL1 Shim\n");

	/* Configure and enable Stage-1 MMU, enable D-Cache */
	shim_plat_configure_mmu();
	enable_mmu_el1(0);

	shim_print_memory_layout();

	return 0;
}
