/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cactus_message_loop.h"
#include "cactus_test_cmds.h"
#include <debug.h>
#include <ffa_helpers.h>

CACTUS_CMD_HANDLER(echo_cmd, CACTUS_ECHO_CMD)
{
	uint64_t echo_val = cactus_echo_get_val(*args);

	VERBOSE("Received echo at %x, value %llx.\n", ffa_dir_msg_dest(*args),
						      echo_val);

	return cactus_success_resp(ffa_dir_msg_dest(*args),
				   ffa_dir_msg_source(*args),
				   echo_val);
}

CACTUS_CMD_HANDLER(req_echo_cmd, CACTUS_REQ_ECHO_CMD)
{
	smc_ret_values ffa_ret;
	ffa_vm_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_vm_id_t echo_dest = cactus_req_echo_get_echo_dest(*args);
	uint64_t echo_val = cactus_echo_get_val(*args);

	VERBOSE("%x requested to send echo to %x, value %llx\n",
		ffa_dir_msg_source(*args), echo_dest, echo_val);

	ffa_ret = cactus_echo_send_cmd(vm_id, echo_dest, echo_val);

	if (!is_ffa_direct_response(ffa_ret)) {
		return cactus_error_resp(vm_id, ffa_dir_msg_source(*args),
					 CACTUS_ERROR_FFA_CALL);
	}

	if (cactus_get_response(ffa_ret) != CACTUS_SUCCESS ||
	    cactus_echo_get_val(ffa_ret) != echo_val) {
		ERROR("Echo Failed!\n");
		return cactus_error_resp(vm_id, ffa_dir_msg_source(*args),
					 CACTUS_ERROR_TEST);
	}

	return cactus_success_resp(vm_id, ffa_dir_msg_source(*args), 0);
}

static smc_ret_values base_deadlock_handler(ffa_vm_id_t vm_id,
					    ffa_vm_id_t source,
					    ffa_vm_id_t deadlock_dest,
					    ffa_vm_id_t deadlock_next_dest)
{
	smc_ret_values ffa_ret;

	ffa_ret = cactus_deadlock_send_cmd(vm_id, deadlock_dest,
					   deadlock_next_dest);

	/*
	 * Should be true for the last partition to attempt
	 * an FF-A direct message, to the first partition.
	 */
	bool is_deadlock_detected = (ffa_func_id(ffa_ret) == FFA_ERROR) &&
				    (ffa_error_code(ffa_ret) == FFA_ERROR_BUSY);

	/*
	 * Should be true after the deadlock has been detected and after the
	 * first response has been sent down the request chain.
	 */
	bool is_returning_from_deadlock =
		(is_ffa_direct_response(ffa_ret)) &&
		(cactus_get_response(ffa_ret) == CACTUS_SUCCESS);

	if (is_deadlock_detected) {
		VERBOSE("Attempt to create deadlock failed\n");
	}

	if (is_deadlock_detected || is_returning_from_deadlock) {
		/*
		 * This is not the partition, that would have created the
		 * deadlock. As such, reply back to the partitions.
		 */
		return cactus_success_resp(vm_id, source, 0);
	}

	/* Shouldn't get to this point */
	ERROR("Deadlock test went wrong!\n");
	return cactus_error_resp(vm_id, source, CACTUS_ERROR_TEST);
}

CACTUS_CMD_HANDLER(deadlock_cmd, CACTUS_DEADLOCK_CMD)
{
	ffa_vm_id_t source = ffa_dir_msg_source(*args);
	ffa_vm_id_t deadlock_dest = cactus_deadlock_get_next_dest(*args);
	ffa_vm_id_t deadlock_next_dest = source;

	VERBOSE("%x is creating deadlock. next: %x\n", source, deadlock_dest);

	return base_deadlock_handler(ffa_dir_msg_dest(*args), source,
				      deadlock_dest, deadlock_next_dest);
}

CACTUS_CMD_HANDLER(req_deadlock_cmd, CACTUS_REQ_DEADLOCK_CMD)
{
	ffa_vm_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_vm_id_t source = ffa_dir_msg_source(*args);
	ffa_vm_id_t deadlock_dest = cactus_deadlock_get_next_dest(*args);
	ffa_vm_id_t deadlock_next_dest = cactus_deadlock_get_next_dest2(*args);

	VERBOSE("%x requested deadlock with %x and %x\n",
		ffa_dir_msg_source(*args), deadlock_dest, deadlock_next_dest);

	return base_deadlock_handler(vm_id, source, deadlock_dest, deadlock_next_dest);
}
