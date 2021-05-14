/*
 * Copyright (c) 2018-2022, Arm Limited. All rights reserved.
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
	ffa_id_t my_id;

	set_putc_impl(SVC_CALL_AS_STDOUT);

	/* Get FF-A id. */
	args = (svc_args){.fid = FFA_ID_GET};
	ret = sp_svc(&args);
	if (ret != FFA_SUCCESS_SMC32) {
		ERROR("Cannot get FF-A id.\n");
		panic();
	}
	my_id = (ffa_id_t)args.arg2;

	NOTICE("Booting Secure Partition (ID: %x)\n", my_id);
	NOTICE("%s\n", build_message);
	NOTICE("%s\n", version_string);

init:
	args = (svc_args) {.fid = FFA_MSG_WAIT};
	ret = sp_svc(&args);

	while (1) {
		ffa_id_t req_sender = (ffa_id_t)(args.arg1 >> 16);

		if (ret != FFA_MSG_SEND_DIRECT_REQ_SMC32) {
			ERROR("unknown FF-A request %lx\n", ret);
			goto init;
		}

		VERBOSE("Received request: %lx\n", args.arg3);


		args.fid = FFA_MSG_SEND_DIRECT_RESP_SMC32;
		args.arg1 = ((u_register_t)my_id) << 16 |
			    (u_register_t)req_sender;
		args.arg2 = 0;
		args.arg3 = 0;

		ret = sp_svc(&args);
	}
}
