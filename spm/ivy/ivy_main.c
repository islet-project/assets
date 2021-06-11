/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <drivers/console.h>
#include <drivers/arm/pl011.h>
#include <errno.h>
#include <ivy_def.h>
#include <plat_arm.h>
#include <platform_def.h>
#include <sp_helpers.h>

#include "ivy.h"
#include "ivy_def.h"

/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

static void ivy_print_memory_layout(void)
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

void __dead2 ivy_main(void)
{
	u_register_t ret;
	svc_args args;

	console_init(PL011_UART3_BASE,
		     PL011_UART3_CLK_IN_HZ,
		     PL011_BAUDRATE);

	NOTICE("Booting test Secure Partition Ivy\n");
	NOTICE("%s\n", build_message);
	NOTICE("%s\n", version_string);
	NOTICE("Running at S-EL0\n");

	ivy_print_memory_layout();

init:
	args = (svc_args){.fid = FFA_MSG_WAIT};
	ret = sp_svc(&args);
	while (1) {
		if (ret != FFA_MSG_SEND_DIRECT_REQ_SMC32) {
			ERROR("unknown FF-A request %lx\n", ret);
			goto init;
		}
		VERBOSE("Received request: %lx\n", args.arg3);
		args.fid = FFA_MSG_SEND_DIRECT_RESP_SMC32;
		args.arg1 = 0x80020000;
		args.arg2 = 0;
		args.arg3 = 0;
		ret = sp_svc(&args);
	}
}
