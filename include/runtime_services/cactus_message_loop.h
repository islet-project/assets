/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
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
	struct ffa_value (*fn)(const struct ffa_value *args,
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
	static struct ffa_value CACTUS_HANDLER_FN_NAME(name)(		\
		const struct ffa_value *args, struct mailbox_buffers *mb)

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

bool cactus_handle_cmd(struct ffa_value *cmd_args, struct ffa_value *ret,
		       struct mailbox_buffers *mb);
