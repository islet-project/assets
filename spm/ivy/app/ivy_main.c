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
#include "sp_tests.h"

/* Host machine information injected by the build system in the ELF file. */
extern const char build_message[];
extern const char version_string[];

void __dead2 ivy_main(void)
{
	struct ffa_value ret;
	ffa_id_t my_id;
	struct mailbox_buffers mb;

	set_putc_impl(SVC_CALL_AS_STDOUT);

	/* Get FF-A id. */
	ret = ffa_id_get();
	if (ffa_func_id(ret) != FFA_SUCCESS_SMC32) {
		ERROR("Cannot get FF-A id.\n");
		panic();
	}
	my_id = ffa_endpoint_id(ret);

	NOTICE("Booting Secure Partition (ID: %x)\n", my_id);
	NOTICE("%s\n", build_message);
	NOTICE("%s\n", version_string);

init:
	VERBOSE("Mapping RXTX Regions\n");
	CONFIGURE_AND_MAP_MAILBOX(mb, PAGE_SIZE, ret);
	if (ffa_func_id(ret) != FFA_SUCCESS_SMC32) {
		ERROR("Failed to map RXTX buffers. Error %x\n",
		      ffa_error_code(ret));
		panic();
	}

	ffa_tests(&mb);

	ret = ffa_msg_wait();

	while (1) {
		if (ffa_func_id(ret) != FFA_MSG_SEND_DIRECT_REQ_SMC32) {
			ERROR("unknown FF-A request %x\n", ffa_func_id(ret));
			goto init;
		}

		VERBOSE("Received request: %lx\n", ret.ret3);

		ret = ffa_msg_send_direct_resp32(my_id, ffa_dir_msg_source(ret),
						 0, 0, 0, 0, 0);
	}
}
