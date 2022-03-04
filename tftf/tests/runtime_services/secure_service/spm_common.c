/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_test_cmds.h>
#include <debug.h>
#include <ffa_endpoints.h>
#include <ffa_svc.h>
#include <lib/extensions/sve.h>
#include <spm_common.h>
#include <xlat_tables_v2.h>

#define __STR(x) #x
#define STR(x) __STR(x)

#define fill_simd_helper(num1, num2) "ldp q"#num1", q"#num2",\
	[%0], #"STR(2 * SIMD_VECTOR_LEN_BYTES)";"
#define read_simd_helper(num1, num2) "stp q"#num1", q"#num2",\
	[%0], #"STR(2 * SIMD_VECTOR_LEN_BYTES)";"

/**
 * Helper to log errors after FF-A calls.
 */
bool is_ffa_call_error(smc_ret_values ret)
{
	if (ffa_func_id(ret) == FFA_ERROR) {
		VERBOSE("FF-A call returned error (%x): %d\n",
		      ffa_func_id(ret), ffa_error_code(ret));
		return true;
	}
	return false;
}

bool is_expected_ffa_error(smc_ret_values ret, int32_t error_code)
{
	if (ffa_func_id(ret) == FFA_ERROR &&
	    ffa_error_code(ret) == error_code) {
		return true;
	}

	ERROR("Expected FFA_ERROR(%x), code: %d, got %x %d\n",
	      FFA_ERROR, error_code, ffa_func_id(ret), ffa_error_code(ret));

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

	VERBOSE("%x is not FF-A response.\n", ffa_func_id(ret));
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

	VERBOSE("Expecting %x, FF-A return was %x\n", func_id, ffa_func_id(ret));

	return false;
}

bool is_expected_cactus_response(smc_ret_values ret, uint32_t expected_resp,
				 uint32_t arg)
{
	if (!is_ffa_direct_response(ret)) {
		return false;
	}

	if (cactus_get_response(ret) != expected_resp ||
	    (uint32_t)ret.ret4 != arg) {
		ERROR("Expected response %x and %x; "
		      "Obtained %x and %x\n",
		      expected_resp, arg, cactus_get_response(ret),
		      (int32_t)ret.ret4);
		return false;
	}

	return true;
}

void dump_smc_ret_values(smc_ret_values ret)
{
	NOTICE("FF-A value: %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx\n",
		ret.ret0,
		ret.ret1,
		ret.ret2,
		ret.ret3,
		ret.ret4,
		ret.ret5,
		ret.ret6,
		ret.ret7);
}

void fill_simd_vector_regs(const simd_vector_t v[SIMD_NUM_VECTORS])
{
#ifdef __aarch64__
	__asm__ volatile(
		fill_simd_helper(0, 1)
		fill_simd_helper(2, 3)
		fill_simd_helper(4, 5)
		fill_simd_helper(6, 7)
		fill_simd_helper(8, 9)
		fill_simd_helper(10, 11)
		fill_simd_helper(12, 13)
		fill_simd_helper(14, 15)
		fill_simd_helper(16, 17)
		fill_simd_helper(18, 19)
		fill_simd_helper(20, 21)
		fill_simd_helper(22, 23)
		fill_simd_helper(24, 25)
		fill_simd_helper(26, 27)
		fill_simd_helper(28, 29)
		fill_simd_helper(30, 31)
		"sub %0, %0, #" STR(SIMD_NUM_VECTORS * SIMD_VECTOR_LEN_BYTES) ";"
		: : "r" (v));
#endif
}

void read_simd_vector_regs(simd_vector_t v[SIMD_NUM_VECTORS])
{
#ifdef __aarch64__
	memset(v, 0, sizeof(simd_vector_t) * SIMD_NUM_VECTORS);
	__asm__ volatile(
		read_simd_helper(0, 1)
		read_simd_helper(2, 3)
		read_simd_helper(4, 5)
		read_simd_helper(6, 7)
		read_simd_helper(8, 9)
		read_simd_helper(10, 11)
		read_simd_helper(12, 13)
		read_simd_helper(14, 15)
		read_simd_helper(16, 17)
		read_simd_helper(18, 19)
		read_simd_helper(20, 21)
		read_simd_helper(22, 23)
		read_simd_helper(24, 25)
		read_simd_helper(26, 27)
		read_simd_helper(28, 29)
		read_simd_helper(30, 31)
		"sub %0, %0, #" STR(SIMD_NUM_VECTORS * SIMD_VECTOR_LEN_BYTES) ";"
		: : "r" (v));
#endif
}

void fill_sve_vector_regs(const sve_vector_t v[SVE_NUM_VECTORS])
{
#ifdef __aarch64__
	__asm__ volatile(
		".arch_extension sve\n"
		fill_sve_helper(0)
		fill_sve_helper(1)
		fill_sve_helper(2)
		fill_sve_helper(3)
		fill_sve_helper(4)
		fill_sve_helper(5)
		fill_sve_helper(6)
		fill_sve_helper(7)
		fill_sve_helper(8)
		fill_sve_helper(9)
		fill_sve_helper(10)
		fill_sve_helper(11)
		fill_sve_helper(12)
		fill_sve_helper(13)
		fill_sve_helper(14)
		fill_sve_helper(15)
		fill_sve_helper(16)
		fill_sve_helper(17)
		fill_sve_helper(18)
		fill_sve_helper(19)
		fill_sve_helper(20)
		fill_sve_helper(21)
		fill_sve_helper(22)
		fill_sve_helper(23)
		fill_sve_helper(24)
		fill_sve_helper(25)
		fill_sve_helper(26)
		fill_sve_helper(27)
		fill_sve_helper(28)
		fill_sve_helper(29)
		fill_sve_helper(30)
		fill_sve_helper(31)
		".arch_extension nosve\n"
		: : "r" (v));
#endif
}

void read_sve_vector_regs(sve_vector_t v[SVE_NUM_VECTORS])
{
#ifdef __aarch64__
	memset(v, 0, sizeof(sve_vector_t) * SVE_NUM_VECTORS);
	__asm__ volatile(
		".arch_extension sve\n"
		read_sve_helper(0)
		read_sve_helper(1)
		read_sve_helper(2)
		read_sve_helper(3)
		read_sve_helper(4)
		read_sve_helper(5)
		read_sve_helper(6)
		read_sve_helper(7)
		read_sve_helper(8)
		read_sve_helper(9)
		read_sve_helper(10)
		read_sve_helper(11)
		read_sve_helper(12)
		read_sve_helper(13)
		read_sve_helper(14)
		read_sve_helper(15)
		read_sve_helper(16)
		read_sve_helper(17)
		read_sve_helper(18)
		read_sve_helper(19)
		read_sve_helper(20)
		read_sve_helper(21)
		read_sve_helper(22)
		read_sve_helper(23)
		read_sve_helper(24)
		read_sve_helper(25)
		read_sve_helper(26)
		read_sve_helper(27)
		read_sve_helper(28)
		read_sve_helper(29)
		read_sve_helper(30)
		read_sve_helper(31)
		".arch_extension nosve\n"
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
	 * FFA direct message. Expect it to implement either v1.0 or v1.1.
	 */
	ret_values = ffa_msg_send_direct_req32(HYP_ID, SP_ID(1),
					       OPTEE_FFA_GET_API_VERSION, 0,
					       0, 0, 0);
	if (ret_values.ret3 == 1 &&
	    (ret_values.ret4 == 0 || ret_values.ret4 == 1)) {
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
	{"FFA_RXTX_UNMAP_32 check", FFA_RXTX_UNMAP, FFA_SUCCESS_SMC32},
	{"FFA_PARTITION_INFO_GET_32 check", FFA_PARTITION_INFO_GET, FFA_SUCCESS_SMC32},
	{"FFA_ID_GET_32 check", FFA_ID_GET, FFA_SUCCESS_SMC32},
	{"FFA_SPM_ID_GET_32 check", FFA_SPM_ID_GET, FFA_SUCCESS_SMC32,
		MAKE_FFA_VERSION(1, 1)},
	{"FFA_MSG_WAIT_32 check", FFA_MSG_WAIT, FFA_SUCCESS_SMC32},
	{"FFA_RUN_32 check", FFA_MSG_RUN, FFA_SUCCESS_SMC32},
	{"FFA_MEM_DONATE_32 check", FFA_MEM_DONATE_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_MEM_LEND_32 check", FFA_MEM_LEND_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_MEM_SHARE_32 check", FFA_MEM_SHARE_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_MEM_RETRIEVE_REQ_32 check", FFA_MEM_RETRIEVE_REQ_SMC32, FFA_SUCCESS_SMC32},
	{"FFA_MEM_RETRIEVE_RESP_32 check", FFA_MEM_RETRIEVE_RESP, FFA_SUCCESS_SMC32},
	{"FFA_MEM_RELINQUISH_32 check", FFA_MEM_RELINQUISH, FFA_SUCCESS_SMC32},
	{"FFA_MEM_RECLAIM_32 check", FFA_MEM_RECLAIM, FFA_SUCCESS_SMC32},
	{"FFA_NOTIFICATION_BITMAP_CREATE_32 check",
		FFA_NOTIFICATION_BITMAP_CREATE, FFA_SUCCESS_SMC32},
	{"FFA_NOTIFICATION_BITMAP_DESTROY_32 check",
		FFA_NOTIFICATION_BITMAP_DESTROY, FFA_SUCCESS_SMC32},
	{"FFA_NOTIFICATION_BIND_32 check", FFA_NOTIFICATION_BIND,
		FFA_SUCCESS_SMC32},
	{"FFA_NOTIFICATION_UNBIND_32 check", FFA_NOTIFICATION_UNBIND,
		FFA_SUCCESS_SMC32},
	{"FFA_NOTIFICATION_SET_32 check", FFA_NOTIFICATION_SET,
		FFA_SUCCESS_SMC32},
	{"FFA_NOTIFICATION_INFO_GET_64 check", FFA_NOTIFICATION_INFO_GET_SMC64,
		FFA_SUCCESS_SMC32},
	/* Indirect messaging is only supported in Nwd */
	{"FFA_YIELD_32 check", FFA_MSG_YIELD, FFA_ERROR},
	{"FFA_MSG_SEND_32 check", FFA_MSG_SEND, FFA_ERROR},
	{"FFA_MSG_POLL_32 check", FFA_MSG_POLL, FFA_ERROR},
	{"Check non-existent command", 0xFFFF, FFA_ERROR},
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
		     ffa_memory_region_flags_t flags)
{
	smc_ret_values ret;
	uint32_t fragment_size;
	uint32_t total_size;
	uint32_t descriptor_size;

	if (retrieved == NULL || mb == NULL) {
		ERROR("Invalid parameters!\n");
		return false;
	}

	descriptor_size = ffa_memory_retrieve_request_init(
	    mb->send, handle, sender, receiver, 0, flags,
	    FFA_DATA_ACCESS_RW,
	    FFA_INSTRUCTION_ACCESS_NX,
	    FFA_MEMORY_NORMAL_MEM,
	    FFA_MEMORY_CACHE_WRITE_BACK,
	    FFA_MEMORY_INNER_SHAREABLE);

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
 * FFA_MEMORY_HANDLE_INVALID if something goes wrong. Populates *ret with a
 * resulting smc value to handle the error higher in the test chain.
 *
 * TODO: Do memory send with 'ffa_memory_region' taking multiple segments
 */
ffa_memory_handle_t memory_send(
	struct ffa_memory_region *memory_region, uint32_t mem_func,
	uint32_t fragment_length, uint32_t total_length, smc_ret_values *ret)
{
	ffa_id_t receiver =
		memory_region->receivers[0].receiver_permissions.receiver;

	if (fragment_length != total_length) {
		ERROR("For now, fragment_length and total_length need to be"
		      " equal");
		return FFA_MEMORY_HANDLE_INVALID;
	}

	switch (mem_func) {
	case FFA_MEM_SHARE_SMC32:
		*ret = ffa_mem_share(total_length, fragment_length);
		break;
	case FFA_MEM_LEND_SMC32:
		*ret = ffa_mem_lend(total_length, fragment_length);
		break;
	case FFA_MEM_DONATE_SMC32:
		*ret = ffa_mem_donate(total_length, fragment_length);
		break;
	default:
		*ret = (smc_ret_values){0};
		ERROR("TFTF - Invalid func id %x!\n", mem_func);
		return FFA_MEMORY_HANDLE_INVALID;
	}

	if (is_ffa_call_error(*ret)) {
		ERROR("Failed to send message to: %x\n", receiver);
		return FFA_MEMORY_HANDLE_INVALID;
	}

	return ffa_mem_success_handle(*ret);
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
	uint32_t constituents_count, uint32_t mem_func, smc_ret_values *ret)
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
			       total_length, ret);
}

/**
 * Sends a ffa_partition_info request and checks the response against the
 * target.
 */
bool ffa_partition_info_helper(struct mailbox_buffers *mb,
			       const struct ffa_uuid uuid,
			       const struct ffa_partition_info *expected,
			       const uint16_t expected_size)
{
	bool result = true;
	smc_ret_values ret = ffa_partition_info_get(uuid);

	if (ffa_func_id(ret) == FFA_SUCCESS_SMC32) {
		if (ffa_partition_info_count(ret) != expected_size) {
			ERROR("Unexpected number of partitions %ld\n", ret.ret2);
			return false;
		}
		const struct ffa_partition_info *info =
			(const struct ffa_partition_info *)(mb->recv);

		for (unsigned int i = 0U; i < expected_size; i++) {
			if (info[i].id != expected[i].id) {
				ERROR("Wrong ID. Expected %x, got %x\n",
				      expected[i].id,
				      info[i].id);
				result = false;
			}
			if (info[i].exec_context != expected[i].exec_context) {
				ERROR("Wrong context. Expected %d, got %d\n",
				      expected[i].exec_context,
				      info[i].exec_context);
				result = false;
			}
			if (info[i].properties != expected[i].properties) {
				ERROR("Wrong properties. Expected %d, got %d\n",
				      expected[i].properties,
				      info[i].properties);
				result = false;
			}
			if (info[i].uuid.uuid[0] != expected[i].uuid.uuid[0] ||
			    info[i].uuid.uuid[1] != expected[i].uuid.uuid[1] ||
			    info[i].uuid.uuid[2] != expected[i].uuid.uuid[2] ||
			    info[i].uuid.uuid[3] != expected[i].uuid.uuid[3]) {
				ERROR("Wrong UUID. Expected %x %x %x %x, "
				      "got %x %x %x %x\n",
				      expected[i].uuid.uuid[0],
				      expected[i].uuid.uuid[1],
				      expected[i].uuid.uuid[2],
				      expected[i].uuid.uuid[3],
				      info[i].uuid.uuid[0],
				      info[i].uuid.uuid[1],
				      info[i].uuid.uuid[2],
				      info[i].uuid.uuid[3]);
				result = false;
			}
		}
	}

	ret = ffa_rx_release();
	if (is_ffa_call_error(ret)) {
		ERROR("Failed to release RX buffer\n");
		result = false;
	}
	return result;
}
