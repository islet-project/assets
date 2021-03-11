/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ffa_helpers.h>
#include <spm_common.h>

/**
 * Pairs a command id with a function call, to handle the command ID.
 */
struct cactus_cmd_handler {
	const uint64_t id;
	smc_ret_values (*fn)(const smc_ret_values *args,
			     struct mailbox_buffers *mb);
};

/**
 * Helper to create the name of a handler function.
 */
#define CACTUS_HANDLER_FN_NAME(name) cactus_##name##_handler

/**
 * Define handler's function signature.
 */
#define CACTUS_HANDLER_FN(name)						\
	static smc_ret_values CACTUS_HANDLER_FN_NAME(name)(		\
		const smc_ret_values *args, struct mailbox_buffers *mb)

/**
 * Helper to define Cactus command handler, and pair it with a command ID.
 * It also creates a table with this information, to be traversed by
 * 'cactus_handle_cmd' function.
 */
#define CACTUS_CMD_HANDLER(name, ID)					\
	CACTUS_HANDLER_FN(name);					\
	struct cactus_cmd_handler name __section(".cactus_handler") = {	\
		.id = ID, .fn = CACTUS_HANDLER_FN_NAME(name),		\
	};								\
	CACTUS_HANDLER_FN(name)

bool cactus_handle_cmd(smc_ret_values *cmd_args, smc_ret_values *ret,
		       struct mailbox_buffers *mb);
