/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ffa_helpers.h>
#include <test_helpers.h>
#include <tftf_lib.h>

struct feature_test {
	const char *test_name;
	unsigned int feature;
	u_register_t expected_ret;
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

test_result_t test_ffa_features(void)
{
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 0);

	/* Check if SPMC is OP-TEE at S-EL1 */
	if (check_spmc_execution_level()) {
		/* FFA_FEATURES is not yet supported in OP-TEE */
		return TEST_RESULT_SUCCESS;
	}

	smc_ret_values ffa_ret;
	unsigned int i, test_target_size =
		sizeof(test_target) / sizeof(struct feature_test);

	for (i = 0U; i < test_target_size; i++) {
		ffa_ret = ffa_features(test_target[i].feature);
		if (ffa_ret.ret0 != test_target[i].expected_ret) {
			tftf_testcase_printf("%s returned %lx, expected %lx\n",
					     test_target[i].test_name,
					     ffa_ret.ret0,
					     test_target[i].expected_ret);
			return TEST_RESULT_FAIL;
		}
		if ((test_target[i].expected_ret == (u_register_t)FFA_ERROR) &&
		    (ffa_ret.ret2 != (u_register_t)FFA_ERROR_NOT_SUPPORTED)) {
			tftf_testcase_printf("%s failed for the wrong reason: "
					     "returned %lx, expected %lx\n",
					     test_target[i].test_name,
					     ffa_ret.ret2,
					     (u_register_t)FFA_ERROR_NOT_SUPPORTED);
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}
