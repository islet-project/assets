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

CACTUS_CMD_HANDLER(notifications_get, CACTUS_NOTIFICATION_GET_CMD)
{
	ffa_id_t source = ffa_dir_msg_source(*args);
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_id_t notification_receiver =
				cactus_notification_get_receiver(*args);
	uint32_t flags = cactus_notification_get_flags(*args);
	uint32_t vcpu_id = cactus_notification_get_vcpu(*args);
	smc_ret_values ret;

	VERBOSE("Partition %x requested to get notifications.\n", source);

	ret = ffa_notification_get(notification_receiver, vcpu_id, flags);

	if (is_ffa_call_error(ret)) {
		return cactus_error_resp(vm_id, source, ffa_error_code(ret));
	}

	VERBOSE("Notifications returned:\n"
			"   from sp: %llx\n"
			"   from vm: %llx\n",
		ffa_notifications_get_from_sp(ret),
		ffa_notifications_get_from_vm(ret));

	return cactus_notifications_get_success_resp(
		vm_id, source, ffa_notifications_get_from_sp(ret),
		ffa_notifications_get_from_vm(ret));
}

CACTUS_CMD_HANDLER(notifications_set, CACTUS_NOTIFICATIONS_SET_CMD)
{
	ffa_id_t source = ffa_dir_msg_source(*args);
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_id_t receiver = cactus_notification_get_receiver(*args);
	ffa_id_t sender = cactus_notification_get_sender(*args);
	ffa_notification_bitmap_t notifications = cactus_notification_get_notifications(*args);
	uint32_t flags = cactus_notification_get_flags(*args);
	smc_ret_values ret;

	VERBOSE("Partition %x requested to set notifications.\n", source);

	ret = ffa_notification_set(sender, receiver, flags, notifications);

	if (is_ffa_call_error(ret)) {
		return cactus_error_resp(vm_id, source, ffa_error_code(ret));
	}

	return cactus_response(vm_id, source, CACTUS_SUCCESS);
}
