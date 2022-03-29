/*
 * Copyright (c) 2020-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>

#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <spm_common.h>
#include <xlat_tables_defs.h>

#define MAILBOX_SIZE PAGE_SIZE

#define SENDER HYP_ID
#define RECEIVER SP_ID(1)

static const struct ffa_uuid expected_sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}
	};

/* Memory section to be used for memory share operations */
static __aligned(PAGE_SIZE) uint8_t share_page[PAGE_SIZE];

static bool check_written_words(uint32_t *ptr, uint32_t word, uint32_t wcount)
{
	VERBOSE("TFTF - Memory contents after SP use:\n");
	for (unsigned int i = 0U; i < wcount; i++) {
		VERBOSE("      %u: %x\n", i, ptr[i]);

		/* Verify content of memory is as expected. */
		if (ptr[i] != word) {
			return false;
		}
	}
	return true;
}

/**
 * Tests that it is possible to share memory with SWd from NWd.
 * After calling the respective memory send API, it will expect a reply from
 * cactus SP, at which point it will reclaim access to the memory region and
 * check the memory region has been used by receiver SP.
 *
 * Accessing memory before a memory reclaim operation should only be possible
 * in the context of a memory share operation.
 * According to the FF-A spec, the owner is temporarily relinquishing
 * access to the memory region on a memory lend operation, and on a
 * memory donate operation the access is relinquished permanently.
 * SPMC is positioned in S-EL2, and doesn't control stage-1 mapping for
 * EL2. Therefore, it is impossible to enforce the expected access
 * policy for a donate and lend operations within the SPMC.
 * Current SPMC implementation is under the assumption of trust that
 * Hypervisor (sitting in EL2) would relinquish access from EL1/EL0
 * FF-A endpoint at relevant moment.
 */
static test_result_t test_memory_send_sp(uint32_t mem_func)
{
	struct ffa_value ret;
	ffa_memory_handle_t handle;
	uint32_t *ptr;
	struct mailbox_buffers mb;

	/* Arbitrarily write 5 words after using memory. */
	const uint32_t nr_words_to_write = 5;

	/***********************************************************************
	 * Check if SPMC has ffa_version and expected FFA endpoints are deployed.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	GET_TFTF_MAILBOX(mb);

	struct ffa_memory_region_constituent constituents[] = {
						{(void *)share_page, 1, 0}
					};

	const uint32_t constituents_count = sizeof(constituents) /
			sizeof(struct ffa_memory_region_constituent);

	handle = memory_init_and_send((struct ffa_memory_region *)mb.send,
					MAILBOX_SIZE, SENDER, RECEIVER,
					constituents, constituents_count,
					mem_func, &ret);

	if (handle == FFA_MEMORY_HANDLE_INVALID) {
		return TEST_RESULT_FAIL;
	}

	VERBOSE("TFTF - Handle: %llx\nTFTF - Address: %p\n",
					handle, constituents[0].address);

	ptr = (uint32_t *)constituents[0].address;

	ret = cactus_mem_send_cmd(SENDER, RECEIVER, mem_func, handle, 0,
				  true, nr_words_to_write);

	if (!is_ffa_direct_response(ret)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) != CACTUS_SUCCESS) {
		ERROR("Failed memory send operation!\n");
		return TEST_RESULT_FAIL;
	}

	/* Check that borrower used the memory as expected for this test. */
	if (!check_written_words(ptr, mem_func, nr_words_to_write)) {
		ERROR("Words written to shared memory, not as expected.\n");
		return TEST_RESULT_FAIL;
	}

	if (mem_func != FFA_MEM_DONATE_SMC32 &&
	    is_ffa_call_error(ffa_mem_reclaim(handle, 0))) {
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

/*
 * Test requests a memory send operation between cactus SPs.
 * Cactus SP should reply to TFTF on whether the test succeeded or not.
 */
static test_result_t test_req_mem_send_sp_to_sp(uint32_t mem_func,
						ffa_id_t sender_sp,
						ffa_id_t receiver_sp,
						bool non_secure)
{
	struct ffa_value ret;

	/***********************************************************************
	 * Check if SPMC's ffa_version and presence of expected FF-A endpoints.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	ret = cactus_req_mem_send_send_cmd(HYP_ID, sender_sp, mem_func,
					   receiver_sp, non_secure);

	if (!is_ffa_direct_response(ret)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) == CACTUS_ERROR) {
		ERROR("Failed sharing memory between SPs. Error code: %d\n",
			cactus_error_code(ret));
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Test requests a memory send operation from SP to VM.
 * The tests expects cactus to reply CACTUS_ERROR, providing FF-A error code of
 * the last memory send FF-A call that cactus performed.
 */
static test_result_t test_req_mem_send_sp_to_vm(uint32_t mem_func,
						ffa_id_t sender_sp,
						ffa_id_t receiver_vm)
{
	struct ffa_value ret;

	/**********************************************************************
	 * Check if SPMC's ffa_version and presence of expected FF-A endpoints.
	 *********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	ret = cactus_req_mem_send_send_cmd(HYP_ID, sender_sp, mem_func,
					   receiver_vm, false);

	if (!is_ffa_direct_response(ret)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) == CACTUS_ERROR &&
	    cactus_error_code(ret) == FFA_ERROR_DENIED) {
		return TEST_RESULT_SUCCESS;
	}

	tftf_testcase_printf("Did not get the expected error, "
			     "mem send returned with %d\n",
			     cactus_get_response(ret));
	return TEST_RESULT_FAIL;
}

test_result_t test_req_mem_share_sp_to_sp(void)
{
	return test_req_mem_send_sp_to_sp(FFA_MEM_SHARE_SMC32, SP_ID(3),
					  SP_ID(2), false);
}

test_result_t test_req_ns_mem_share_sp_to_sp(void)
{
	/* Added this peprocessor condition because the test fails when RME is
	 * enabled, because the model has PA_SIZE=48, but still doesn't have
	 * allocated RAM allocatable there nor the itnerconnect support 48bit
	 * addresses. */
#if PA_SIZE == 48
	SKIP_TEST_IF_PA_SIZE_LESS_THAN(48);
	return test_req_mem_send_sp_to_sp(FFA_MEM_SHARE_SMC32, SP_ID(3),
					  SP_ID(2), true);
#else
	return TEST_RESULT_SKIPPED;
#endif
}

test_result_t test_req_mem_lend_sp_to_sp(void)
{
	return test_req_mem_send_sp_to_sp(FFA_MEM_LEND_SMC32, SP_ID(3),
					  SP_ID(2), false);
}

test_result_t test_req_mem_donate_sp_to_sp(void)
{
	return test_req_mem_send_sp_to_sp(FFA_MEM_DONATE_SMC32, SP_ID(1),
					  SP_ID(3), false);
}

test_result_t test_req_mem_share_sp_to_vm(void)
{
	return test_req_mem_send_sp_to_vm(FFA_MEM_SHARE_SMC32, SP_ID(1),
					  HYP_ID);
}

test_result_t test_req_mem_lend_sp_to_vm(void)
{
	return test_req_mem_send_sp_to_vm(FFA_MEM_LEND_SMC32, SP_ID(2),
					  HYP_ID);
}

test_result_t test_mem_share_to_sp_clear_memory(void)
{
	struct ffa_memory_region_constituent constituents[] = {
						{(void *)share_page, 1, 0}};
	const uint32_t constituents_count = sizeof(constituents) /
			sizeof(struct ffa_memory_region_constituent);
	struct mailbox_buffers mb;
	uint32_t remaining_constituent_count;
	uint32_t total_length;
	uint32_t fragment_length;
	ffa_memory_handle_t handle;
	struct ffa_value ret;
	uint32_t *ptr;
	/* Arbitrarily write 10 words after using shared memory. */
	const uint32_t nr_words_to_write = 10U;

	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	GET_TFTF_MAILBOX(mb);

	remaining_constituent_count = ffa_memory_region_init(
		(struct ffa_memory_region *)mb.send, MAILBOX_SIZE, SENDER,
		RECEIVER, constituents, constituents_count, 0,
		FFA_MEMORY_REGION_FLAG_CLEAR, FFA_DATA_ACCESS_RW,
		FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED, FFA_MEMORY_NORMAL_MEM,
		FFA_MEMORY_CACHE_WRITE_BACK, FFA_MEMORY_INNER_SHAREABLE,
		&total_length, &fragment_length);

	if (remaining_constituent_count != 0) {
		ERROR("Transaction descriptor initialization failed!\n");
		return TEST_RESULT_FAIL;
	}

	handle = memory_send(mb.send, FFA_MEM_LEND_SMC32, fragment_length,
			     total_length, &ret);

	if (handle == FFA_MEMORY_HANDLE_INVALID) {
		ERROR("Memory Share failed!\n");
		return TEST_RESULT_FAIL;
	}

	VERBOSE("Memory has been shared!\n");

	ret = cactus_mem_send_cmd(SENDER, RECEIVER, FFA_MEM_LEND_SMC32, handle,
				  FFA_MEMORY_REGION_FLAG_CLEAR, true,
				  nr_words_to_write);

	if (!is_ffa_direct_response(ret)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) != CACTUS_SUCCESS) {
		ERROR("Failed memory send operation!\n");
		return TEST_RESULT_FAIL;
	}

	ret = ffa_mem_reclaim(handle, 0);

	if (is_ffa_call_error(ret)) {
		ERROR("Memory reclaim failed!\n");
		return TEST_RESULT_FAIL;
	}

	ptr = (uint32_t *)constituents[0].address;

	/* Check that borrower used the memory as expected for this test. */
	if (!check_written_words(ptr, FFA_MEM_LEND_SMC32, nr_words_to_write)) {
		ERROR("Words written to shared memory, not as expected.\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
