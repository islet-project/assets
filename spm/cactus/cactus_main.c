/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <console.h>
#include <debug.h>
#include <pl011.h>
#include <plat_arm.h>
#include <platform_def.h>
#include <secure_partition.h>
#include <sp_helpers.h>
#include <spm_svc.h>
#include <std_svc.h>

#include "cactus.h"
#include "cactus_def.h"
#include "cactus_tests.h"

/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

static void cactus_print_memory_layout(void)
{
	NOTICE("Secure Partition memory layout:\n");

	NOTICE("  Image regions\n");
	NOTICE("    Text region            : %p - %p\n",
		(void *)CACTUS_TEXT_START, (void *)CACTUS_TEXT_END);
	NOTICE("    Read-only data region  : %p - %p\n",
		(void *)CACTUS_RODATA_START, (void *)CACTUS_RODATA_END);
	NOTICE("    Data region            : %p - %p\n",
		(void *)CACTUS_DATA_START, (void *)CACTUS_DATA_END);
	NOTICE("    BSS region             : %p - %p\n",
		(void *)CACTUS_BSS_START, (void *)CACTUS_BSS_END);
	NOTICE("    Total image memory     : %p - %p\n",
		(void *)CACTUS_IMAGE_BASE,
		(void *)(CACTUS_IMAGE_BASE + CACTUS_IMAGE_SIZE));
	NOTICE("  SPM regions\n");
	NOTICE("    SPM <-> SP buffer      : %p - %p\n",
		(void *)CACTUS_SPM_BUF_BASE,
		(void *)(CACTUS_SPM_BUF_BASE + CACTUS_SPM_BUF_SIZE));
	NOTICE("    NS <-> SP buffer       : %p - %p\n",
		(void *)CACTUS_NS_BUF_BASE,
		(void *)(CACTUS_NS_BUF_BASE + CACTUS_NS_BUF_SIZE));
	NOTICE("  Test regions\n");
	NOTICE("    Test region            : %p - %p\n",
		(void *)CACTUS_TEST_MEM_BASE,
		(void *)(CACTUS_TEST_MEM_BASE + CACTUS_TEST_MEM_SIZE));
}

void __dead2 cactus_main(void)
{
	console_init(PL011_UART2_BASE,
		     PL011_UART2_CLK_IN_HZ,
		     PL011_BAUDRATE);

	NOTICE("Booting test Secure Partition Cactus\n");
	NOTICE("%s\n", build_message);
	NOTICE("%s\n", version_string);
	NOTICE("Running at S-EL0\n");

	cactus_print_memory_layout();

	/*
	 * Run some initial tests.
	 *
	 * These are executed when the system is still booting, just after SPM
	 * has handed over to Cactus.
	 */
	misc_tests();
	system_setup_tests();
	mem_attr_changes_tests();

	/*
	 * Handle secure service requests.
	 */
	secure_services_loop();
}
