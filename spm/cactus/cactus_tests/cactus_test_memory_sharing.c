/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_def.h>
#include "cactus_message_loop.h"
#include "cactus_test_cmds.h"
#include "cactus_tests.h"
#include <debug.h>
#include <ffa_helpers.h>
#include <sp_helpers.h>
#include <xlat_tables_defs.h>
#include <lib/xlat_tables/xlat_tables_v2.h>

/* Memory section to be used for memory share operations */
static __aligned(PAGE_SIZE) uint8_t share_page[PAGE_SIZE];

CACTUS_CMD_HANDLER(mem_send_cmd, CACTUS_MEM_SEND_CMD)
{
	struct ffa_memory_region *m;
	struct ffa_composite_memory_region *composite;
	int ret;
	unsigned int mem_attrs;
	uint32_t *ptr;
	ffa_vm_id_t source = ffa_dir_msg_source(*args);
	ffa_vm_id_t vm_id = ffa_dir_msg_dest(*args);
	uint32_t mem_func = cactus_req_mem_send_get_mem_func(*args);
	uint64_t handle = cactus_mem_send_get_handle(*args);

	expect(memory_retrieve(mb, &m, handle, source, vm_id, mem_func), true);

	composite = ffa_memory_region_get_composite(m, 0);

	VERBOSE("Address: %p; page_count: %x %x\n",
		composite->constituents[0].address,
		composite->constituents[0].page_count, PAGE_SIZE);

	/* This test is only concerned with RW permissions. */
	if (ffa_get_data_access_attr(
			m->receivers[0].receiver_permissions.permissions) !=
		FFA_DATA_ACCESS_RW) {
		ERROR("Permissions not expected!\n");
		return cactus_error_resp(vm_id, source, CACTUS_ERROR_TEST);
	}

	mem_attrs = MT_RW_DATA | MT_EXECUTE_NEVER;

	if (!IS_SP_ID(source)) {
		mem_attrs |= MT_NS;
	}

	ret = mmap_add_dynamic_region(
			(uint64_t)composite->constituents[0].address,
			(uint64_t)composite->constituents[0].address,
			composite->constituents[0].page_count * PAGE_SIZE,
			mem_attrs);

	if (ret != 0) {
		ERROR("Failed first mmap_add_dynamic_region!\n");
		return cactus_error_resp(vm_id, source, CACTUS_ERROR_TEST);
	}

	VERBOSE("Memory has been mapped\n");

	ptr = (uint32_t *) composite->constituents[0].address;

	/* Write mem_func to retrieved memory region for validation purposes. */
	VERBOSE("Writing: %x\n", mem_func);
	for (unsigned int i = 0U; i < 5U; i++)
		ptr[i] = mem_func;

	/*
	 * A FFA_MEM_DONATE changes the ownership of the page, as such no
	 * relinquish is needed.
	 */
	if (mem_func != FFA_MEM_DONATE_SMC32) {
		ret = mmap_remove_dynamic_region(
			(uint64_t)composite->constituents[0].address,
			composite->constituents[0].page_count * PAGE_SIZE);

		if (ret != 0) {
			ERROR("Failed first mmap_add_dynamic_region!\n");
			return cactus_error_resp(vm_id, source,
						 CACTUS_ERROR_TEST);
		}

		if (!memory_relinquish((struct ffa_mem_relinquish *)mb->send,
					m->handle, vm_id)) {
			return cactus_error_resp(vm_id, source,
						 CACTUS_ERROR_TEST);
		}
	}

	if (ffa_func_id(ffa_rx_release()) != FFA_SUCCESS_SMC32) {
		ERROR("Failed to release buffer!\n");
		return cactus_error_resp(vm_id, source,
					 CACTUS_ERROR_FFA_CALL);
	}

	return cactus_success_resp(vm_id,
				   source, 0);
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

	handle = memory_init_and_send(
		(struct ffa_memory_region *)mb->send, PAGE_SIZE,
		vm_id, receiver, constituents,
		constituents_count, mem_func);

	/*
	 * If returned an invalid handle, we should break the test.
	 */
	if (handle == FFA_MEMORY_HANDLE_INVALID) {
		ERROR("Received an invalid FF-A memory Handle!\n");
		return cactus_error_resp(vm_id, source,
					 CACTUS_ERROR_TEST);
	}

	ffa_ret = cactus_mem_send_cmd(vm_id, receiver, mem_func, handle);

	if (!is_ffa_direct_response(ffa_ret)) {
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
		if (is_ffa_call_error(ffa_ret)) {
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
