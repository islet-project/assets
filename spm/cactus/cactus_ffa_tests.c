/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <cactus_def.h>
#include <ffa_helpers.h>
#include <sp_helpers.h>

/* FFA version test helpers */
#define FFA_MAJOR 1U
#define FFA_MINOR 0U

static const uint32_t primary_uuid[4] = PRIMARY_UUID;
static const uint32_t secondary_uuid[4] = SECONDARY_UUID;
static const uint32_t null_uuid[4] = {0};

struct feature_test {
	const char *test_name;
	unsigned int feature;
	unsigned int expected_ret;
};

static const struct feature_test test_target[] = {
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
 * Test FFA_FEATURES interface.
 */
static void ffa_features_test(void)
{
	const char *test_features = "FFA Features interface";
	smc_ret_values ffa_ret;
	unsigned int i, test_target_size =
		sizeof(test_target) / sizeof(struct feature_test);

	announce_test_section_start(test_features);

	for (i = 0U; i < test_target_size; i++) {
		announce_test_start(test_target[i].test_name);

		ffa_ret = ffa_features(test_target[i].feature);
		expect(ffa_ret.ret0, test_target[i].expected_ret);
		if (test_target[i].expected_ret == FFA_ERROR) {
			expect(ffa_ret.ret2, FFA_ERROR_NOT_SUPPORTED);
		}

		announce_test_end(test_target[i].test_name);
	}

	announce_test_section_end(test_features);
}

static void ffa_partition_info_helper(struct mailbox_buffers *mb, const uint32_t uuid[4],
			       const struct ffa_partition_info *expected,
			       const uint16_t expected_size)
{
	smc_ret_values ret = ffa_partition_info_get(uuid);
	unsigned int i;
	expect(ret.ret0, FFA_SUCCESS_SMC32);

	struct ffa_partition_info *info = (struct ffa_partition_info *)(mb->recv);
	for (i = 0U; i < expected_size; i++) {
		expect(info[i].id, expected[i].id);
		expect(info[i].exec_context, expected[i].exec_context);
		expect(info[i].properties, expected[i].properties);
	}

	ret = ffa_rx_release();
	expect(ret.ret0, FFA_SUCCESS_SMC32);
}

static void ffa_partition_info_wrong_test(void)
{
	const char *test_wrong_uuid = "Request wrong UUID";
	uint32_t uuid[4] = {1};

	announce_test_start(test_wrong_uuid);

	smc_ret_values ret = ffa_partition_info_get(uuid);
	expect(ret.ret0, FFA_ERROR);
	expect(ret.ret2, FFA_ERROR_INVALID_PARAMETER);

	announce_test_end(test_wrong_uuid);
}

static void ffa_partition_info_get_test(struct mailbox_buffers *mb)
{
	const char *test_partition_info = "FFA Partition info interface";
	const char *test_primary = "Get primary partition info";
	const char *test_secondary = "Get secondary partition info";
	const char *test_all = "Get all partitions info";

	const struct ffa_partition_info expected_info[] = {
		{.id = SPM_VM_ID_FIRST, .exec_context = 8, .properties = 0}, /* Primary partition info */
		{.id = 2, .exec_context = 2, .properties = 0} /* Secondary partition info */
	};

	announce_test_section_start(test_partition_info);

	announce_test_start(test_secondary);
	ffa_partition_info_helper(mb, secondary_uuid, &expected_info[1], 1);
	announce_test_end(test_secondary);

	announce_test_start(test_primary);
	ffa_partition_info_helper(mb, primary_uuid, &expected_info[0], 1);
	announce_test_end(test_primary);

	announce_test_start(test_all);
	ffa_partition_info_helper(mb, null_uuid, expected_info, 2);
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

void ffa_tests(struct mailbox_buffers *mb)
{
	const char *test_ffa = "FFA Interfaces";

	announce_test_section_start(test_ffa);

	ffa_features_test();
	ffa_version_test();
	ffa_partition_info_get_test(mb);

	announce_test_section_end(test_ffa);
}
