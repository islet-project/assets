/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cactus_def.h"
#include "cactus_test_cmds.h"
#include "cactus_tests.h"
#include <ffa_helpers.h>
#include <sp_helpers.h>
#include <xlat_tables_defs.h>

/* Memory section to be used for memory share operations */
static __aligned(PAGE_SIZE) uint8_t share_page[PAGE_SIZE];

CACTUS_CMD_HANDLER(mem_send_cmd, CACTUS_MEM_SEND_CMD)
{
	ffa_memory_management_test(
		mb, ffa_dir_msg_dest(*args), ffa_dir_msg_source(*args),
		cactus_req_mem_send_get_mem_func(*args),
		cactus_mem_send_get_handle(*args));

	return cactus_success_resp(ffa_dir_msg_dest(*args),
				   ffa_dir_msg_source(*args), 0);
}

CACTUS_CMD_HANDLER(req_mem_send_cmd, CACTUS_REQ_MEM_SEND_CMD)
{
	smc_ret_values ffa_ret;
	uint32_t mem_func = cactus_req_mem_send_get_mem_func(*args);
	ffa_vm_id_t receiver = cactus_req_mem_send_get_receiver(*args);
	ffa_memory_handle_t handle;
	ffa_vm_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_vm_id_t source = ffa_dir_msg_source(*args);

	VERBOSE("%x requested to send memory to %x (func: %x)\n",
		source, receiver, mem_func);

	const struct ffa_memory_region_constituent constituents[] = {
		{(void *)share_page, 1, 0}
	};

	const uint32_t constituents_count = (sizeof(constituents) /
					     sizeof(constituents[0]));

	handle = ffa_memory_init_and_send(
		(struct ffa_memory_region *)mb->send, PAGE_SIZE,
		vm_id, receiver, constituents,
		constituents_count, mem_func);

	/*
	 * If returned an invalid handle, we should break the test.
	 */
	expect(handle != FFA_MEMORY_HANDLE_INVALID, true);

	ffa_ret = cactus_mem_send_cmd(vm_id, receiver, mem_func, handle);

	if (!is_ffa_direct_response(ffa_ret)) {
		ERROR("Failed to send message. ret: %x error: %x\n",
		      ffa_func_id(ffa_ret),
		      ffa_error_code(ffa_ret));
		return cactus_error_resp(vm_id, source, CACTUS_ERROR_FFA_CALL);
	}

	/* If anything went bad on the receiver's end. */
	if (cactus_get_response(ffa_ret) == CACTUS_ERROR) {
		ERROR("Received error from receiver!\n");
		return cactus_error_resp(vm_id, source, CACTUS_ERROR_TEST);
	}

	if (mem_func != FFA_MEM_DONATE_SMC32) {
		/*
		 * Do a memory reclaim only if the mem_func regards to memory
		 * share or lend operations, as with a donate the owner is
		 * permanently given up access to the memory region.
		 */
		ffa_ret = ffa_mem_reclaim(handle, 0);
		if (ffa_func_id(ffa_ret) == FFA_ERROR) {
			ERROR("Failed to reclaim memory! error: %x\n",
			      ffa_error_code(ffa_ret));
			return cactus_error_resp(vm_id, source,
						 CACTUS_ERROR_TEST);
		}

		/**
		 * Read Content that has been written to memory to validate
		 * access to memory segment has been reestablished, and receiver
		 * made use of memory region.
		 */
		#if (LOG_LEVEL >= LOG_LEVEL_VERBOSE)
			uint32_t *ptr = (uint32_t *)constituents->address;

			VERBOSE("Memory contents after receiver SP's use:\n");
			for (unsigned int i = 0U; i < 5U; i++) {
				VERBOSE("      %u: %x\n", i, ptr[i]);
			}
		#endif
	}

	return cactus_success_resp(vm_id, source, 0);
}
