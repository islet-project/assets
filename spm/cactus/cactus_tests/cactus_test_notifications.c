/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cactus_message_loop.h"
#include "cactus_test_cmds.h"
#include "cactus_tests.h"
#include <ffa_helpers.h>
#include <debug.h>

/* Booleans to keep track of which CPUs handled NPI. */
static bool npi_handled[PLATFORM_CORE_COUNT];

/**
 * Helper to access the above array and set the boolean for the specific CPU.
 */
void set_npi_handled(uint32_t vcpu_id, bool val)
{
	npi_handled[vcpu_id] = val;
}

/**
 * Helper to get state of the boolean from `npi_handled` from the respective
 * CPU.
 */
bool get_npi_handled(uint32_t vcpu_id)
{
	return npi_handled[vcpu_id];
}

void notification_pending_interrupt_handler(void)
{
	/* Get which core it is running from. */
	unsigned int core_pos = platform_get_core_pos(
						read_mpidr_el1() & MPID_MASK);

	VERBOSE("NPI handled in core %u\n", core_pos);

	set_npi_handled(core_pos, true);
}


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

	/* If requested to check the status of NPI, for the respective CPU. */
	if (cactus_notifications_check_npi_handled(*args)) {

		/* If NPI hasn't been handled return error for this test. */
		if (!get_npi_handled(vcpu_id)) {
			return cactus_error_resp(vm_id, source,
						 CACTUS_ERROR_TEST);
		}

		/* Reset NPI flag for the respective core. */
		set_npi_handled(vcpu_id, false);
	}

	return cactus_notifications_get_success_resp(
		vm_id, source, ffa_notifications_get_from_sp(ret),
		ffa_notifications_get_from_vm(ret));
}

CACTUS_CMD_HANDLER(notifications_set, CACTUS_NOTIFICATIONS_SET_CMD)
{
	ffa_id_t source = ffa_dir_msg_source(*args);
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_notification_bitmap_t notifications =
				 cactus_notification_get_notifications(*args);
	ffa_id_t receiver = cactus_notifications_set_get_receiver(*args);
	ffa_id_t sender = cactus_notifications_set_get_sender(*args);
	ffa_id_t echo_dest = cactus_req_echo_get_echo_dest(*args);
	uint32_t flags = cactus_notification_get_flags(*args);
	smc_ret_values ret;

	VERBOSE("Partition %x requested to set notifications.\n", source);

	ret = ffa_notification_set(sender, receiver, flags, notifications);

	if (is_ffa_call_error(ret)) {
		return cactus_error_resp(vm_id, source, ffa_error_code(ret));
	}

	/*
	 * If flag to delay notification pending interrupt, an echo test command
	 * should be sent to another SP, to validate SWd is not preempted.
	 */
	if ((flags & FFA_NOTIFICATIONS_FLAG_DELAY_SRI) != 0 &&
	    IS_SP_ID(echo_dest)) {
		VERBOSE("Delay SRI. Test Echo to %x.\n", echo_dest);
		ret = cactus_echo_send_cmd(vm_id, echo_dest,
					   FFA_NOTIFICATION_SET);

		if (!is_expected_cactus_response(ret, CACTUS_SUCCESS,
						 FFA_NOTIFICATION_SET)) {
			ERROR("Echo Failed!\n");
			return cactus_error_resp(vm_id, source,
						 CACTUS_ERROR_TEST);
		}
	}

	VERBOSE("Set notifications handled (core %u)!\n", get_current_core_id());

	return cactus_response(vm_id, source, CACTUS_SUCCESS);
}
