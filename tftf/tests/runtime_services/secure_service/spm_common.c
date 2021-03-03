/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <ffa_endpoints.h>
#include <spm_common.h>
#include <xlat_tables_v2.h>

#define __STR(x) #x
#define STR(x) __STR(x)
#define SIMD_TWO_VECTORS_BYTES_STR	(2 * SIMD_VECTOR_LEN_BYTES)

/**
 * Helper to log errors after FF-A calls.
 */
bool is_ffa_call_error(smc_ret_values ret)
{
	if (ffa_func_id(ret) == FFA_ERROR) {
		ERROR("FF-A call returned error (%x): %d\n",
		      ffa_func_id(ret), ffa_error_code(ret));
		return true;
	}
	return false;
}

/**
 * Helper to verify return of FF-A call is an FFA_MSG_SEND_DIRECT_RESP.
 * Should be used after FFA_MSG_SEND_DIRECT_REQ, or after sending a test command
 * to an SP.
 */
bool is_ffa_direct_response(smc_ret_values ret)
{
	if ((ffa_func_id(ret) == FFA_MSG_SEND_DIRECT_RESP_SMC32) ||
	    (ffa_func_id(ret) == FFA_MSG_SEND_DIRECT_RESP_SMC64)) {
		return true;
	}

	ERROR("%x is not FF-A response.\n", ffa_func_id(ret));
	/* To log error in case it is FFA_ERROR*/
	is_ffa_call_error(ret);

	return false;
}

/**
 * Helper to check the return value of FF-A call is as expected.
 */
bool is_expected_ffa_return(smc_ret_values ret, uint32_t func_id)
{
	if (ffa_func_id(ret) == func_id) {
		return true;
	}

	ERROR("Expecting %x, FF-A return was %x\n", func_id, ffa_func_id(ret));

	return false;
}

void fill_simd_vector_regs(const simd_vector_t v[SIMD_NUM_VECTORS])
{
#ifdef __aarch64__
	__asm__ volatile(
		"ldp q0, q1, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q2, q3, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q4, q5, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q6, q7, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q8, q9, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q10, q11, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q12, q13, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q14, q15, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q16, q17, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q18, q19, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q20, q21, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q22, q23, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q24, q25, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q26, q27, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q28, q29, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"ldp q30, q31, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"sub %0, %0, #" STR(SIMD_NUM_VECTORS * SIMD_VECTOR_LEN_BYTES) ";"
		: : "r" (v));
#endif
}

void read_simd_vector_regs(simd_vector_t v[SIMD_NUM_VECTORS])
{
#ifdef __aarch64__
	memset(v, 0, sizeof(simd_vector_t) * SIMD_NUM_VECTORS);

	__asm__ volatile(
		"stp q0, q1, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q2, q3, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q4, q5, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q6, q7, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q8, q9, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q10, q11, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q12, q13, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q14, q15, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q16, q17, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q18, q19, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q20, q21, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q22, q23, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q24, q25, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q26, q27, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q28, q29, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"stp q30, q31, [%0], #" STR(SIMD_TWO_VECTORS_BYTES_STR) ";"
		"sub %0, %0, #" STR(SIMD_NUM_VECTORS * SIMD_VECTOR_LEN_BYTES) ";"
		: : "r" (v));
#endif
}

/*
 * check_spmc_execution_level
 *
 * Attempt sending impdef protocol messages to OP-TEE through direct messaging.
 * Criteria for detecting OP-TEE presence is that responses match defined
 * version values. In the case of SPMC running at S-EL2 (and Cactus instances
 * running at S-EL1) the response will not match the pre-defined version IDs.
 *
 * Returns true if SPMC is probed as being OP-TEE at S-EL1.
 *
 */
bool check_spmc_execution_level(void)
{
	unsigned int is_optee_spmc_criteria = 0U;
	smc_ret_values ret_values;

	/*
	 * Send a first OP-TEE-defined protocol message through
	 * FFA direct message.
	 */
	ret_values = ffa_msg_send_direct_req32(HYP_ID, SP_ID(1),
					       OPTEE_FFA_GET_API_VERSION, 0,
					       0, 0, 0);
	if ((ret_values.ret3 == FFA_VERSION_MAJOR) &&
	    (ret_values.ret4 == FFA_VERSION_MINOR)) {
		is_optee_spmc_criteria++;
	}

	/*
	 * Send a second OP-TEE-defined protocol message through
	 * FFA direct message.
	 */
	ret_values = ffa_msg_send_direct_req32(HYP_ID, SP_ID(1),
					       OPTEE_FFA_GET_OS_VERSION,
					       0, 0, 0, 0);
	if ((ret_values.ret3 == OPTEE_FFA_GET_OS_VERSION_MAJOR) &&
	    (ret_values.ret4 == OPTEE_FFA_GET_OS_VERSION_MINOR)) {
		is_optee_spmc_criteria++;
	}

	return (is_optee_spmc_criteria == 2U);
}

static const struct ffa_features_test ffa_feature_test_target[] = {
	{"FFA_ERROR_32 check", FFA_ERROR, FFA_SUCCESS_SMC32},
	{"FFA_SUCCESS_32 check", FFA_SUCCESS_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_INTERRUPT_32 check", FFA_INTERRUPT, FFA_SUCCESS_SMC32},
	{"FFA_VERSION_32 check", FFA_VERSION, FFA_SUCCESS_SMC32},
	{"FFA_FEATURES_32 check", FFA_FEATURES, FFA_SUCCESS_SMC32},
	{"FFA_RX_RELEASE_32 check", FFA_RX_RELEASE, FFA_SUCCESS_SMC32},
	{"FFA_RXTX_MAP_32 check", FFA_RXTX_MAP_SMC32, FFA_ERROR},
	{"FFA_RXTX_MAP_64 check", FFA_RXTX_MAP_SMC64, FFA_SUCCESS_SMC32},
	{"FFA_RXTX_UNMAP_32 check", FFA_RXTX_UNMAP, FFA_ERROR},
	{"FFA_PARTITION_INFO_GET_32 check", FFA_PARTITION_INFO_GET, FFA_SUCCESS_SMC32},
	{"FFA_ID_GET_32 check", FFA_ID_GET, FFA_SUCCESS_SMC32},
	{"FFA_MSG_POLL_32 check", FFA_MSG_POLL, FFA_SUCCESS_SMC32},
	{"FFA_MSG_WAIT_32 check", FFA_MSG_WAIT, FFA_SUCCESS_SMC32},
	{"FFA_YIELD_32 check", FFA_MSG_YIELD, FFA_SUCCESS_SMC32},
	{"FFA_RUN_32 check", FFA_MSG_RUN, FFA_SUCCESS_SMC32},
	{"FFA_MSG_SEND_32 check", FFA_MSG_SEND, FFA_SUCCESS_SMC32},
	{"FFA_MEM_DONATE_32 check", FFA_MEM_DONATE_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_MEM_LEND_32 check", FFA_MEM_LEND_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_MEM_SHARE_32 check", FFA_MEM_SHARE_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_MEM_RETRIEVE_REQ_32 check", FFA_MEM_RETRIEVE_REQ_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_MEM_RETRIEVE_RESP_32 check", FFA_MEM_RETRIEVE_RESP, FFA_SUCCESS_SMC32},
	{"FFA_MEM_RELINQUISH_32 check", FFA_MEM_RELINQUISH, FFA_SUCCESS_SMC32},
	{"FFA_MEM_RECLAIM_32 check", FFA_MEM_RECLAIM, FFA_SUCCESS_SMC32},
	{"Check non-existent command", 0xFFFF, FFA_ERROR}
};

/*
 * Populates test_target with content of ffa_feature_test_target.
 *
 * Returns number of elements in the *test_target.
 */
unsigned int get_ffa_feature_test_target(
	const struct ffa_features_test **test_target)
{
	if (test_target != NULL) {
		*test_target = ffa_feature_test_target;
	}

	return sizeof(ffa_feature_test_target) /
	       sizeof(struct ffa_features_test);
}

bool memory_retrieve(struct mailbox_buffers *mb,
		     struct ffa_memory_region **retrieved, uint64_t handle,
		     ffa_id_t sender, ffa_id_t receiver,
		     uint32_t mem_func)
{
	smc_ret_values ret;
	uint32_t fragment_size;
	uint32_t total_size;
	uint32_t descriptor_size;

	if (retrieved == NULL || mb == NULL) {
		ERROR("Invalid parameters!\n");
		return false;
	}

	/*
	 * TODO: Revise shareability attribute in function call
	 * below.
	 * https://lists.trustedfirmware.org/pipermail/hafnium/2020-June/000023.html
	 */
	descriptor_size = ffa_memory_retrieve_request_init(
	    mb->send, handle, sender, receiver, 0, 0,
	    FFA_DATA_ACCESS_RW,
	    FFA_INSTRUCTION_ACCESS_NX,
	    FFA_MEMORY_NORMAL_MEM,
	    FFA_MEMORY_CACHE_WRITE_BACK,
	    FFA_MEMORY_OUTER_SHAREABLE);

	ret = ffa_mem_retrieve_req(descriptor_size, descriptor_size);

	if (ffa_func_id(ret) != FFA_MEM_RETRIEVE_RESP) {
		ERROR("Couldn't retrieve the memory page. Error: %x\n",
		      ffa_error_code(ret));
		return false;
	}

	/*
	 * Following total_size and fragment_size are useful to keep track
	 * of the state of transaction. When the sum of all fragment_size of all
	 * fragments is equal to total_size, the memory transaction has been
	 * completed.
	 * This is a simple test with only one segment. As such, upon
	 * successful ffa_mem_retrieve_req, total_size must be equal to
	 * fragment_size.
	 */
	total_size = ret.ret1;
	fragment_size = ret.ret2;

	if (total_size != fragment_size) {
		ERROR("Only expect one memory segment to be sent!\n");
		return false;
	}

	if (fragment_size > PAGE_SIZE) {
		ERROR("Fragment should be smaller than RX buffer!\n");
		return false;
	}

	*retrieved = (struct ffa_memory_region *)mb->recv;

	if ((*retrieved)->receiver_count > MAX_MEM_SHARE_RECIPIENTS) {
		VERBOSE("SPMC memory sharing operations support max of %u "
			"receivers!\n", MAX_MEM_SHARE_RECIPIENTS);
		return false;
	}

	VERBOSE("Memory Retrieved!\n");

	return true;
}

bool memory_relinquish(struct ffa_mem_relinquish *m, uint64_t handle,
		       ffa_id_t id)
{
	smc_ret_values ret;

	ffa_mem_relinquish_init(m, handle, 0, id);
	ret = ffa_mem_relinquish();
	if (ffa_func_id(ret) != FFA_SUCCESS_SMC32) {
		ERROR("%s failed to relinquish memory! error: %x\n",
		      __func__, ffa_error_code(ret));
		return false;
	}

	VERBOSE("Memory Relinquished!\n");
	return true;
}

/**
 * Helper to call memory send function whose func id is passed as a parameter.
 * Returns a valid handle in case of successful operation or
 * FFA_MEMORY_HANDLE_INVALID if something goes wrong.
 *
 * TODO: Do memory send with 'ffa_memory_region' taking multiple segments
 */
ffa_memory_handle_t memory_send(
	struct ffa_memory_region *memory_region, uint32_t mem_func,
	uint32_t fragment_length, uint32_t total_length)
{
	smc_ret_values ret;
	ffa_id_t receiver =
		memory_region->receivers[0].receiver_permissions.receiver;

	if (fragment_length != total_length) {
		ERROR("For now, fragment_length and total_length need to be"
		      " equal");
		return FFA_MEMORY_HANDLE_INVALID;
	}

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
		ERROR("TFTF - Invalid func id %x!\n", mem_func);
		return FFA_MEMORY_HANDLE_INVALID;
	}

	if (is_ffa_call_error(ret)) {
		ERROR("Failed to send message to: %x\n", receiver);
		return FFA_MEMORY_HANDLE_INVALID;
	}

	return ffa_mem_success_handle(ret);
}

/**
 * Helper that initializes and sends a memory region. The memory region's
 * configuration is statically defined and is implementation specific. However,
 * doing it in this file for simplicity and for testing purposes.
 */
ffa_memory_handle_t memory_init_and_send(
	struct ffa_memory_region *memory_region, size_t memory_region_max_size,
	ffa_id_t sender, ffa_id_t receiver,
	const struct ffa_memory_region_constituent *constituents,
	uint32_t constituents_count, uint32_t mem_func)
{
	uint32_t remaining_constituent_count;
	uint32_t total_length;
	uint32_t fragment_length;

	enum ffa_data_access data_access = (mem_func == FFA_MEM_DONATE_SMC32) ?
						FFA_DATA_ACCESS_NOT_SPECIFIED :
						FFA_DATA_ACCESS_RW;

	remaining_constituent_count = ffa_memory_region_init(
		memory_region, memory_region_max_size, sender, receiver, constituents,
		constituents_count, 0, 0, data_access,
		FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
		FFA_MEMORY_NORMAL_MEM, FFA_MEMORY_CACHE_WRITE_BACK,
		FFA_MEMORY_INNER_SHAREABLE, &total_length, &fragment_length
	);

	/*
	 * For simplicity of the test, and at least for the time being,
	 * the following condition needs to be true.
	 */
	if (remaining_constituent_count != 0U) {
		ERROR("Remaining constituent should be 0\n");
		return FFA_MEMORY_HANDLE_INVALID;
	}

	return memory_send(memory_region, mem_func, fragment_length,
			       total_length);
}
