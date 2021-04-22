/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_def.h>
#include <cactus_platform_def.h>
#include "cactus_message_loop.h"
#include "cactus_test_cmds.h"
#include "cactus_tests.h"
#include <debug.h>
#include <ffa_helpers.h>
#include <sp_helpers.h>
#include <xlat_tables_defs.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <sync.h>

static volatile uint32_t data_abort_gpf_triggered;

static bool data_abort_gpf_handler(void)
{
	uint64_t esr_el1 = read_esr_el1();

	VERBOSE("%s count %u esr_el1 %llx elr_el1 %llx\n",
		__func__, data_abort_gpf_triggered, esr_el1,
		read_elr_el1());

	/* Expect a data abort because of a GPF. */
	if ((EC_BITS(esr_el1) == EC_DABORT_CUR_EL) &&
	    ((ISS_BITS(esr_el1) & ISS_DFSC_MASK) == DFSC_GPF_DABORT)) {
		data_abort_gpf_triggered++;
		return true;
	}

	return false;
}

/**
 * Each Cactus SP has a memory region dedicated to memory sharing tests
 * described in their partition manifest.
 * This function returns the expected base address depending on the
 * SP ID (should be the same as the manifest).
 */
static void *share_page(ffa_id_t cactus_sp_id)
{
	switch (cactus_sp_id) {
	case SP_ID(1):
		return (void *)CACTUS_SP1_MEM_SHARE_BASE;
	case SP_ID(2):
		return (void *)CACTUS_SP2_MEM_SHARE_BASE;
	case SP_ID(3):
		return (void *)CACTUS_SP3_MEM_SHARE_BASE;
	default:
		ERROR("Helper function expecting a valid Cactus SP ID!\n");
		panic();
	}
}

CACTUS_CMD_HANDLER(mem_send_cmd, CACTUS_MEM_SEND_CMD)
{
	struct ffa_memory_region *m;
	struct ffa_composite_memory_region *composite;
	int ret;
	unsigned int mem_attrs;
	uint32_t *ptr;
	ffa_id_t source = ffa_dir_msg_source(*args);
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	uint32_t mem_func = cactus_req_mem_send_get_mem_func(*args);
	uint64_t handle = cactus_mem_send_get_handle(*args);
	ffa_memory_region_flags_t retrv_flags =
					 cactus_mem_send_get_retrv_flags(*args);
	uint32_t words_to_write = cactus_mem_send_words_to_write(*args);

	expect(memory_retrieve(mb, &m, handle, source, vm_id,
			       retrv_flags), true);

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
		ERROR("Failed to map received memory region(%d)!\n", ret);
		return cactus_error_resp(vm_id, source, CACTUS_ERROR_TEST);
	}

	VERBOSE("Memory has been mapped\n");

	ptr = (uint32_t *) composite->constituents[0].address;

	/* Check that memory has been cleared by the SPMC before using it. */
	if ((retrv_flags & FFA_MEMORY_REGION_FLAG_CLEAR) != 0U) {
		VERBOSE("Check if memory has been cleared!\n");
		for (uint32_t i = 0; i < words_to_write; i++) {
			if (ptr[i] != 0) {
				/*
				 * If it hasn't been cleared, shouldn't be used.
				 */
				ERROR("Memory should have been cleared!\n");
				return cactus_error_resp(
					vm_id, source, CACTUS_ERROR_TEST);
			}
		}
	}

	data_abort_gpf_triggered = 0;
	register_custom_sync_exception_handler(data_abort_gpf_handler);

	/* Write mem_func to retrieved memory region for validation purposes. */
	VERBOSE("Writing: %x\n", mem_func);
	for (unsigned int i = 0U; i < words_to_write; i++) {
		ptr[i] = mem_func;
	}

	unregister_custom_sync_exception_handler();

	/*
	 * A FFA_MEM_DONATE changes the ownership of the page, as such no
	 * relinquish is needed.
	 */
	if (mem_func != FFA_MEM_DONATE_SMC32) {
		ret = mmap_remove_dynamic_region(
			(uint64_t)composite->constituents[0].address,
			composite->constituents[0].page_count * PAGE_SIZE);

		if (ret != 0) {
			ERROR("Failed to unmap received memory region(%d)!\n", ret);
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
				   source, data_abort_gpf_triggered);
}

CACTUS_CMD_HANDLER(req_mem_send_cmd, CACTUS_REQ_MEM_SEND_CMD)
{
	smc_ret_values ffa_ret;
	uint32_t mem_func = cactus_req_mem_send_get_mem_func(*args);
	ffa_id_t receiver = cactus_req_mem_send_get_receiver(*args);
	ffa_memory_handle_t handle;
	ffa_id_t vm_id = ffa_dir_msg_dest(*args);
	ffa_id_t source = ffa_dir_msg_source(*args);
	int ret;
	static bool share_memory_mapped;

	VERBOSE("%x requested to send memory to %x (func: %x), page: %llx\n",
		source, receiver, mem_func, (uint64_t)share_page(vm_id));

	const struct ffa_memory_region_constituent constituents[] = {
		{share_page(vm_id), 1, 0}
	};

	const uint32_t constituents_count = (sizeof(constituents) /
					     sizeof(constituents[0]));

	if (!share_memory_mapped) {
		ret = mmap_add_dynamic_region(
			(uint64_t)constituents[0].address,
			(uint64_t)constituents[0].address,
			constituents[0].page_count * PAGE_SIZE,
			MT_RW_DATA);

		if (ret != 0) {
			ERROR("Failed map share memory before sending (%d)!\n",
			      ret);
			return cactus_error_resp(vm_id, source,
						 CACTUS_ERROR_TEST);
		}
		share_memory_mapped = true;
	}

	handle = memory_init_and_send(
		(struct ffa_memory_region *)mb->send, PAGE_SIZE,
		vm_id, receiver, constituents,
		constituents_count, mem_func, &ffa_ret);

	/*
	 * If returned an invalid handle, we should break the test.
	 */
	if (handle == FFA_MEMORY_HANDLE_INVALID) {
		VERBOSE("Received an invalid FF-A memory Handle!\n");
		return cactus_error_resp(vm_id, source,
					 ffa_error_code(ffa_ret));
	}

	ffa_ret = cactus_mem_send_cmd(vm_id, receiver, mem_func, handle, 0, 10);

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
	} else {
		ret = mmap_remove_dynamic_region(
			(uint64_t)constituents[0].address,
			constituents[0].page_count * PAGE_SIZE);

		if (ret != 0) {
			ERROR("Failed to unmap donated region (%d)!\n", ret);
			return cactus_error_resp(vm_id, source,
						 CACTUS_ERROR_TEST);
		}
	}

	return cactus_success_resp(vm_id, source, 0);
}
