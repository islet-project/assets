/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cactus_message_loop.h"
#include "cactus_test_cmds.h"
#include "cactus_tests.h"
#include <debug.h>
#include <ffa_helpers.h>

CACTUS_CMD_HANDLER(notifications_bind, CACTUS_NOTIFICATION_BIND_CMD)
{
	ffa_id_t source = ffa_dir_msg_source(*args);
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_id_t receiver = cactus_notification_get_receiver(*args);
	ffa_id_t sender = cactus_notification_get_sender(*args);
	ffa_notification_bitmap_t notifications =
		cactus_notification_get_notifications(*args);
	uint32_t flags = cactus_notification_get_flags(*args);
	smc_ret_values ret;

	VERBOSE("Partition %x requested to bind notifications '%llx' to %x\n",
		source, notifications, receiver);

	ret = ffa_notification_bind(sender, receiver, flags, notifications);

	if (is_ffa_call_error(ret)) {
		return cactus_error_resp(vm_id, source, ffa_error_code(ret));
	}

	return cactus_response(vm_id, source, CACTUS_SUCCESS);
}

CACTUS_CMD_HANDLER(notifications_unbind, CACTUS_NOTIFICATION_UNBIND_CMD)
{
	ffa_id_t source = ffa_dir_msg_source(*args);
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_id_t receiver = cactus_notification_get_receiver(*args);
	ffa_id_t sender = cactus_notification_get_sender(*args);
	ffa_notification_bitmap_t notifications =
		cactus_notification_get_notifications(*args);
	smc_ret_values ret;

	VERBOSE("Partition %x requested to unbind notifications '%llx' to %x\n",
	     source, notifications, receiver);

	ret = ffa_notification_unbind(sender, receiver, notifications);

	if (is_ffa_call_error(ret)) {
		return cactus_error_resp(vm_id, source, ffa_error_code(ret));
	}

	return cactus_response(vm_id, source, CACTUS_SUCCESS);
}
