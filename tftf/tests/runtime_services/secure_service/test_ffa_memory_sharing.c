/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_test_cmds.h>
#include <debug.h>
#include <ffa_endpoints.h>
#include <ffa_helpers.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <xlat_tables_defs.h>

#define MAILBOX_SIZE PAGE_SIZE

#define SENDER HYP_ID
#define RECEIVER SP_ID(1)

/* Memory section to be sent over mem management ABIs */
static __aligned(PAGE_SIZE) uint8_t share_page[PAGE_SIZE];

static test_result_t test_memory_send_sp(uint32_t mem_func)
{
	smc_ret_values ret;
	uint32_t remaining_constituent_count;
	uint32_t total_length;
	uint32_t fragment_length;
	uint32_t sent_length;
	ffa_memory_handle_t handle;
	uint32_t *ptr;
	struct mailbox_buffers mb;
	const uint32_t primary_uuid[] = PRIMARY_UUID;

	/**********************************************************************
	 * Verify that FFA is there and that it has the correct version.
	 **********************************************************************/
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 0);

	/**********************************************************************
	 * If OPTEE is SPMC skip this test.
	 **********************************************************************/
	if (check_spmc_execution_level()) {
		VERBOSE("OPTEE as SPMC at S-EL1. Skipping test!\n");
		return TEST_RESULT_SKIPPED;
	}

	if (!get_tftf_mailbox(&mb)) {
		ERROR("Mailbox not configured!\n This test relies on"
		      " test suite \"FF-A RXTX Mapping\" to map/configure"
		      " RXTX buffers\n");
		return TEST_RESULT_FAIL;
	}


	/**********************************************************************
	 * Verify that cactus primary SP is deployed in the system.
	 **********************************************************************/
	SKIP_TEST_IF_FFA_ENDPOINT_NOT_DEPLOYED(mb, primary_uuid);

	struct ffa_memory_region_constituent constituents[] = {
						{(void *)share_page, 1, 0}
					};

	const uint32_t constituents_count = sizeof(constituents) /
			sizeof(struct ffa_memory_region_constituent);

	enum ffa_data_access data_access = (mem_func == FFA_MEM_DONATE_SMC32) ?
						FFA_DATA_ACCESS_NOT_SPECIFIED :
						FFA_DATA_ACCESS_RW;

	/*
	 * TODO: Revise shareability attribute in function call
	 * below.
	 * https://lists.trustedfirmware.org/pipermail/hafnium/2020-June/000023.html
	 */
	remaining_constituent_count = ffa_memory_region_init(
		mb.send, MAILBOX_SIZE, SENDER, RECEIVER, constituents,
		constituents_count, 0, 0,
		data_access,
		FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
		FFA_MEMORY_NORMAL_MEM,
		FFA_MEMORY_CACHE_WRITE_BACK,
		FFA_MEMORY_OUTER_SHAREABLE,
		&total_length,
		&fragment_length
	);

	switch (mem_func) {
	case FFA_MEM_SHARE_SMC32:
		ret = ffa_mem_share(total_length, fragment_length);
		break;
	case FFA_MEM_LEND_SMC32:
		ret = ffa_mem_lend(total_length, fragment_length);
		break;
	case FFA_MEM_DONATE_SMC32:
		ret = ffa_mem_donate(total_length, fragment_length);
		break;
	default:
		NOTICE("TFTF - Invalid func id!\n");
		return TEST_RESULT_FAIL;
	}

	sent_length = fragment_length;

	if (ret.ret0 != FFA_SUCCESS_SMC32) {
		tftf_testcase_printf("Failed to send memory to SP %x.\n",
				      RECEIVER);
		return TEST_RESULT_FAIL;
	}

	if (sent_length != total_length) {
		tftf_testcase_printf("Sent and Total lengths must be equal!\n");
		return TEST_RESULT_FAIL;
	}

	if (remaining_constituent_count != 0) {
		tftf_testcase_printf("Remaining constituent should be 0\n");
		return TEST_RESULT_FAIL;
	}

	handle = ffa_mem_success_handle(ret);

	VERBOSE("TFTF - Handle: %llx\nTFTF - Address: %p\n",
					handle, constituents[0].address);

	ptr = (uint32_t *)constituents[0].address;

	ret = CACTUS_MEM_SEND_CMD(SENDER, RECEIVER, mem_func, handle);

	if (ret.ret0 != FFA_MSG_SEND_DIRECT_RESP_SMC32) {
		ERROR("Failed to send message. error: %lx\n",
		      ret.ret2);
		return TEST_RESULT_FAIL;
	}

	if (CACTUS_GET_RESPONSE(ret) != CACTUS_SUCCESS) {
		tftf_testcase_printf("Failed memory send operation!\n");
		return TEST_RESULT_FAIL;
	}

	/*
	 * Print 5 words from the memory region to validate SP wrote to the
	 * memory region.
	 */
	VERBOSE("TFTF - Memory contents after SP use:\n");
	for (unsigned int i = 0U; i < 5U; i++)
		VERBOSE("      %u: %x\n", i, ptr[i]);

	/* To make the compiler happy in case it is not a verbose build */
	if (LOG_LEVEL < LOG_LEVEL_VERBOSE)
		(void)ptr;

	if (mem_func != FFA_MEM_DONATE_SMC32 &&
	    ffa_mem_reclaim(handle, 0).ret0 == FFA_ERROR) {
			tftf_testcase_printf("Couldn't reclaim memory\n");
			return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

test_result_t test_mem_share_sp(void)
{
	return test_memory_send_sp(FFA_MEM_SHARE_SMC32);
}

test_result_t test_mem_lend_sp(void)
{
	return test_memory_send_sp(FFA_MEM_LEND_SMC32);
}

test_result_t test_mem_donate_sp(void)
{
	return test_memory_send_sp(FFA_MEM_DONATE_SMC32);
}
