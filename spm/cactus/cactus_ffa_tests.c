/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include <cactus_def.h>
#include <cactus_platform_def.h>
#include <ffa_endpoints.h>
#include <sp_helpers.h>
#include <spm_common.h>

#include <lib/libc/string.h>
#include <lib/xlat_tables/xlat_tables_v2.h>

/* FFA version test helpers */
#define FFA_MAJOR 1U
#define FFA_MINOR 0U

static const uint32_t primary_uuid[4] = PRIMARY_UUID;
static const uint32_t secondary_uuid[4] = SECONDARY_UUID;
static const uint32_t tertiary_uuid[4] = TERTIARY_UUID;
static const uint32_t null_uuid[4] = {0};

/*
 * Fill SIMD vectors from secure world side with a unique value.
 * 0x22 is just a dummy value to be distinguished from the value
 * in the normal world.
 */
void fill_simd_vectors(void)
{
	simd_vector_t simd_vectors[SIMD_NUM_VECTORS];

	for (unsigned int num = 0U; num < SIMD_NUM_VECTORS; num++) {
		memset(simd_vectors[num], 0x22 * num, sizeof(simd_vector_t));
	}

	fill_simd_vector_regs(simd_vectors);
}

/*
 * Test FFA_FEATURES interface.
 */
static void ffa_features_test(void)
{
	const char *test_features = "FFA Features interface";
	smc_ret_values ffa_ret;
	const struct ffa_features_test *ffa_feature_test_target;
	unsigned int i, test_target_size =
		get_ffa_feature_test_target(&ffa_feature_test_target);


	announce_test_section_start(test_features);

	for (i = 0U; i < test_target_size; i++) {
		announce_test_start(ffa_feature_test_target[i].test_name);

		ffa_ret = ffa_features(ffa_feature_test_target[i].feature);
		expect(ffa_func_id(ffa_ret), ffa_feature_test_target[i].expected_ret);
		if (ffa_feature_test_target[i].expected_ret == FFA_ERROR) {
			expect(ffa_error_code(ffa_ret), FFA_ERROR_NOT_SUPPORTED);
		}

		announce_test_end(ffa_feature_test_target[i].test_name);
	}

	announce_test_section_end(test_features);
}

static void ffa_partition_info_helper(struct mailbox_buffers *mb, const uint32_t uuid[4],
			       const struct ffa_partition_info *expected,
			       const uint16_t expected_size)
{
	smc_ret_values ret = ffa_partition_info_get(uuid);
	unsigned int i;
	expect(ffa_func_id(ret), FFA_SUCCESS_SMC32);

	struct ffa_partition_info *info = (struct ffa_partition_info *)(mb->recv);
	for (i = 0U; i < expected_size; i++) {
		expect(info[i].id, expected[i].id);
		expect(info[i].exec_context, expected[i].exec_context);
		expect(info[i].properties, expected[i].properties);
	}

	ret = ffa_rx_release();
	expect(ffa_func_id(ret), FFA_SUCCESS_SMC32);
}

static void ffa_partition_info_wrong_test(void)
{
	const char *test_wrong_uuid = "Request wrong UUID";
	uint32_t uuid[4] = {1};

	announce_test_start(test_wrong_uuid);

	smc_ret_values ret = ffa_partition_info_get(uuid);
	expect(ffa_func_id(ret), FFA_ERROR);
	expect(ffa_error_code(ret), FFA_ERROR_INVALID_PARAMETER);

	announce_test_end(test_wrong_uuid);
}

static void ffa_partition_info_get_test(struct mailbox_buffers *mb)
{
	const char *test_partition_info = "FFA Partition info interface";
	const char *test_primary = "Get primary partition info";
	const char *test_secondary = "Get secondary partition info";
	const char *test_tertiary = "Get tertiary partition info";
	const char *test_all = "Get all partitions info";

	const struct ffa_partition_info expected_info[] = {
		/* Primary partition info */
		{
			.id = SPM_VM_ID_FIRST,
			.exec_context = CACTUS_PRIMARY_EC_COUNT,
			/* Supports receipt of direct message requests. */
			.properties = 1U
		},
		/* Secondary partition info */
		{
			.id = SPM_VM_ID_FIRST + 1U,
			.exec_context = CACTUS_SECONDARY_EC_COUNT,
			.properties = 1U
		},
		/* Tertiary partition info */
		{
			.id = SPM_VM_ID_FIRST + 2U,
			.exec_context = CACTUS_TERTIARY_EC_COUNT,
			.properties = 1U
		}
	};

	announce_test_section_start(test_partition_info);

	announce_test_start(test_tertiary);
	ffa_partition_info_helper(mb, tertiary_uuid, &expected_info[2], 1);
	announce_test_end(test_tertiary);

	announce_test_start(test_secondary);
	ffa_partition_info_helper(mb, secondary_uuid, &expected_info[1], 1);
	announce_test_end(test_secondary);

	announce_test_start(test_primary);
	ffa_partition_info_helper(mb, primary_uuid, &expected_info[0], 1);
	announce_test_end(test_primary);

	announce_test_start(test_all);
	ffa_partition_info_helper(mb, null_uuid, expected_info, 3);
	announce_test_end(test_all);

	ffa_partition_info_wrong_test();

	announce_test_section_end(test_partition_info);
}

void ffa_version_test(void)
{
	const char *test_ffa_version = "FFA Version interface";

	announce_test_start(test_ffa_version);

	smc_ret_values ret = ffa_version(MAKE_FFA_VERSION(FFA_MAJOR, FFA_MINOR));
	uint32_t spm_version = (uint32_t)ret.ret0;

	bool ffa_version_compatible =
		((spm_version >> FFA_VERSION_MAJOR_SHIFT) == FFA_MAJOR &&
		 (spm_version & FFA_VERSION_MINOR_MASK) >= FFA_MINOR);

	NOTICE("FFA_VERSION returned %u.%u; Compatible: %i\n",
		spm_version >> FFA_VERSION_MAJOR_SHIFT,
		spm_version & FFA_VERSION_MINOR_MASK,
		(int)ffa_version_compatible);

	expect((int)ffa_version_compatible, (int)true);

	announce_test_end(test_ffa_version);
}

bool ffa_memory_retrieve_test(struct mailbox_buffers *mb,
			 struct ffa_memory_region **retrieved,
			 uint64_t handle, ffa_vm_id_t sender,
			 ffa_vm_id_t receiver, uint32_t mem_func)
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
		ERROR("Couldn't retrieve the memory page. Error: %lx\n",
		      ret.ret2);
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

bool ffa_memory_relinquish_test(struct ffa_mem_relinquish *m,
			   uint64_t handle,
			   ffa_vm_id_t id)
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

void ffa_memory_management_test(struct mailbox_buffers *mb, ffa_vm_id_t vm_id,
				ffa_vm_id_t sender, uint32_t mem_func,
				uint64_t handle)
{
	const char *test_ffa = "Memory Management";
	struct ffa_memory_region *m;
	struct ffa_composite_memory_region *composite;
	int ret;
	unsigned int mem_attrs;
	uint32_t *ptr;

	announce_test_section_start(test_ffa);

	expect(ffa_memory_retrieve_test(
				mb, &m, handle, sender, vm_id, mem_func),
		true);

	composite = ffa_memory_region_get_composite(m, 0);

	VERBOSE("Address: %p; page_count: %x %x\n",
		composite->constituents[0].address,
		composite->constituents[0].page_count, PAGE_SIZE);

	/* This test is only concerned with RW permissions. */
	expect(ffa_get_data_access_attr(
			m->receivers[0].receiver_permissions.permissions),
		FFA_DATA_ACCESS_RW);

	mem_attrs = MT_RW_DATA | MT_EXECUTE_NEVER;

	if (!IS_SP_ID(sender)) {
		mem_attrs |= MT_NS;
	}

	ret = mmap_add_dynamic_region(
			(uint64_t)composite->constituents[0].address,
			(uint64_t)composite->constituents[0].address,
			composite->constituents[0].page_count * PAGE_SIZE,
			mem_attrs);
	expect(ret, 0);

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
		expect(ret, 0);

		expect(ffa_memory_relinquish_test(
			   (struct ffa_mem_relinquish *)mb->send,
			   m->handle, vm_id),
		       true);
	}

	expect(ffa_func_id(ffa_rx_release()), FFA_SUCCESS_SMC32);

	announce_test_section_end(test_ffa);
}

void ffa_tests(struct mailbox_buffers *mb)
{
	const char *test_ffa = "FFA Interfaces";

	announce_test_section_start(test_ffa);

	ffa_features_test();
	ffa_version_test();
	ffa_partition_info_get_test(mb);

	announce_test_section_end(test_ffa);
}
