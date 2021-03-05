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
#include <spm_helpers.h>
#include <spm_common.h>

#include <lib/libc/string.h>

/* FFA version test helpers */
#define FFA_MAJOR 1U
#define FFA_MINOR 0U

static uint32_t spm_version;

static const struct ffa_uuid sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}
	};
static const struct ffa_uuid null_uuid = { .uuid = {0} };

static const struct ffa_partition_info ffa_expected_partition_info[] = {
	/* Primary partition info */
	{
		.id = SP_ID(1),
		.exec_context = PRIMARY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_DIRECT_REQ_RECV | FFA_PARTITION_DIRECT_REQ_SEND)
	},
	/* Secondary partition info */
	{
		.id = SP_ID(2),
		.exec_context = SECONDARY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_DIRECT_REQ_RECV | FFA_PARTITION_DIRECT_REQ_SEND)
	},
	/* Tertiary partition info */
	{
		.id = SP_ID(3),
		.exec_context = TERTIARY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_DIRECT_REQ_RECV | FFA_PARTITION_DIRECT_REQ_SEND)
	},
	/* Ivy partition info */
	{
		.id = SP_ID(4),
		.exec_context = IVY_EXEC_CTX_COUNT,
		.properties = (FFA_PARTITION_DIRECT_REQ_RECV | FFA_PARTITION_DIRECT_REQ_SEND)
	}
};

/*
 * Test FFA_FEATURES interface.
 */
static void ffa_features_test(void)
{
	const char *test_features = "FFA Features interface";
	smc_ret_values ffa_ret;
	unsigned int expected_ret;
	const struct ffa_features_test *ffa_feature_test_target;
	unsigned int i, test_target_size =
		get_ffa_feature_test_target(&ffa_feature_test_target);
	struct ffa_features_test test_target;


	announce_test_section_start(test_features);

	for (i = 0U; i < test_target_size; i++) {
		test_target = ffa_feature_test_target[i];

		announce_test_start(test_target.test_name);

		ffa_ret = ffa_features(test_target.feature);
		expected_ret = FFA_VERSION_COMPILED
				>= test_target.version_added ?
				test_target.expected_ret : FFA_ERROR;

		expect(ffa_func_id(ffa_ret), expected_ret);
		if (expected_ret == FFA_ERROR) {
			expect(ffa_error_code(ffa_ret), FFA_ERROR_NOT_SUPPORTED);
		}

		announce_test_end(test_target.test_name);
	}

	announce_test_section_end(test_features);
}

static void ffa_partition_info_wrong_test(void)
{
	const char *test_wrong_uuid = "Request wrong UUID";
	const struct ffa_uuid uuid = { .uuid = {1} };

	announce_test_start(test_wrong_uuid);

	smc_ret_values ret = ffa_partition_info_get(uuid);
	expect(ffa_func_id(ret), FFA_ERROR);
	expect(ffa_error_code(ret), FFA_ERROR_INVALID_PARAMETER);

	announce_test_end(test_wrong_uuid);
}

static void ffa_partition_info_get_test(struct mailbox_buffers *mb)
{
	const char *test_partition_info = "FFA Partition info interface";

	announce_test_section_start(test_partition_info);

	expect(ffa_partition_info_helper(mb, sp_uuids[2],
		&ffa_expected_partition_info[2], 1), true);

	expect(ffa_partition_info_helper(mb, sp_uuids[1],
		&ffa_expected_partition_info[1], 1), true);

	expect(ffa_partition_info_helper(mb, sp_uuids[0],
		&ffa_expected_partition_info[0], 1), true);

	expect(ffa_partition_info_helper(mb, null_uuid,
		ffa_expected_partition_info, 4), true);

	ffa_partition_info_wrong_test();

	announce_test_section_end(test_partition_info);
}

void ffa_version_test(void)
{
	const char *test_ffa_version = "FFA Version interface";

	announce_test_start(test_ffa_version);

	smc_ret_values ret = ffa_version(MAKE_FFA_VERSION(FFA_MAJOR, FFA_MINOR));
	spm_version = (uint32_t)ret.ret0;

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

void ffa_spm_id_get_test(void)
{
	const char *test_spm_id_get = "FFA_SPM_ID_GET SMC Function";

	announce_test_start(test_spm_id_get);

	if (spm_version >= MAKE_FFA_VERSION(1, 1)) {
		smc_ret_values ret = ffa_spm_id_get();

		expect(ffa_func_id(ret), FFA_SUCCESS_SMC32);

		ffa_id_t spm_id = ffa_endpoint_id(ret);

		VERBOSE("SPM ID = 0x%x\n", spm_id);
		/*
		 * Check the SPMC value given in the fvp_spmc_manifest
		 * is returned.
		 */
		expect(spm_id, SPMC_ID);
	} else {
		NOTICE("FFA_SPM_ID_GET not supported in this version of FF-A."
			" Test skipped.\n");
	}
	announce_test_end(test_spm_id_get);
}

void ffa_tests(struct mailbox_buffers *mb)
{
	const char *test_ffa = "FFA Interfaces";

	announce_test_section_start(test_ffa);

	ffa_features_test();
	ffa_version_test();
	ffa_spm_id_get_test();
	ffa_partition_info_get_test(mb);

	announce_test_section_end(test_ffa);
}
