/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <spm_common.h>
#include <test_helpers.h>
#include <tftf_lib.h>

test_result_t test_ffa_features(void)
{
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 0);

	/* Check if SPMC is OP-TEE at S-EL1 */
	if (check_spmc_execution_level()) {
		/* FFA_FEATURES is not yet supported in OP-TEE */
		return TEST_RESULT_SUCCESS;
	}

	smc_ret_values ffa_ret;
	const struct ffa_features_test *ffa_feature_test_target;
	unsigned int i, test_target_size =
		get_ffa_feature_test_target(&ffa_feature_test_target);

	for (i = 0U; i < test_target_size; i++) {
		ffa_ret = ffa_features(ffa_feature_test_target[i].feature);
		if (ffa_func_id(ffa_ret) != ffa_feature_test_target[i].expected_ret) {
			tftf_testcase_printf("%s returned %x, expected %x\n",
					     ffa_feature_test_target[i].test_name,
					     ffa_func_id(ffa_ret),
					     ffa_feature_test_target[i].expected_ret);
			return TEST_RESULT_FAIL;
		}
		if ((ffa_feature_test_target[i].expected_ret == FFA_ERROR) &&
		    (ffa_error_code(ffa_ret) != FFA_ERROR_NOT_SUPPORTED)) {
			tftf_testcase_printf("%s failed for the wrong reason: "
					     "returned %x, expected %x\n",
					     ffa_feature_test_target[i].test_name,
					     ffa_error_code(ffa_ret),
					     FFA_ERROR_NOT_SUPPORTED);
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}
