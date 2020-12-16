/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <ffa_helpers.h>
#include <sp_debug.h>
#include <sp_helpers.h>

#include "ivy.h"

/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

void __dead2 ivy_main(void)
{
	u_register_t ret;
	svc_args args;

	set_putc_impl(SVC_CALL_AS_STDOUT);

	NOTICE("Entering S-EL0 Secure Partition\n");
	NOTICE("%s\n", build_message);
	NOTICE("%s\n", version_string);

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
