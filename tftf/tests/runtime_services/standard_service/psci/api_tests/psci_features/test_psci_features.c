/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <psci.h>
#include <smccc.h>
#include <tftf_lib.h>

/*
 * @Test_Aim@ Check the list of PSCI functions for PSCI support
 *
 * Call PSCI_FEATURES for each PSCI function ID.
 * - If a PSCI function is mandatory (as per the PSCI specification) then check
 *   the validity of the return flags.
 * - If a PSCI function is optional (as per the PSCI specification) and
 *   implemented, check the validity of the feature flags.
 */
test_result_t test_psci_features(void)
{
	test_result_t result = TEST_RESULT_SUCCESS;
	int32_t ret_flag;
	const psci_function_t *psci_fn;

	for (unsigned int i = 0; i < PSCI_NUM_CALLS; i++) {
		psci_fn = &psci_functions[i];

		ret_flag = tftf_get_psci_feature_info(psci_fn->id);

		if (!psci_fn->mandatory) {
			/*
			 * If the PSCI function is optional then the PSCI
			 * implementation is allowed to not implement it.
			 */
			if (ret_flag == PSCI_E_NOT_SUPPORTED)
				continue;

			INFO("%s non-mandatory function is SUPPORTED\n",
				psci_fn->str);
		} else {
			/* Check mandatory PSCI call is supported */
			if (ret_flag == PSCI_E_NOT_SUPPORTED) {
				result = TEST_RESULT_FAIL;
				tftf_testcase_printf(
					"%s mandatory function is NOT SUPPORTED\n",
					psci_fn->str);
				continue;
			}
		}

		/* Check the feature flags for CPU_SUSPEND PSCI calls */
		if ((psci_fn->id == SMC_PSCI_CPU_SUSPEND_AARCH32) ||
		    (psci_fn->id == SMC_PSCI_CPU_SUSPEND_AARCH64)) {
			if ((ret_flag & ~CPU_SUSPEND_FEAT_VALID_MASK) != 0) {
				result = TEST_RESULT_FAIL;
				tftf_testcase_printf(
					"Invalid feature flags for CPU_SUSPEND: 0x%x\n",
					ret_flag);
			}
		} else {
			/* Check the feature flags for other PSCI calls */
			if (ret_flag != PSCI_FEATURE_FLAGS_ZERO) {
				result = TEST_RESULT_FAIL;
				tftf_testcase_printf(
					"Wrong feature flags for %s\n, "
					"expected 0x%08x, got 0x%08x\n",
					psci_fn->str,
					PSCI_FEATURE_FLAGS_ZERO, ret_flag);
			}
		}
	}

	return result;
}

/*
 * @Test_Aim@ Check invalid PSCI function ids (Negative Test).
 */
test_result_t test_psci_features_invalid_id(void)
{
	/* Invalid function ids for negative testing */
	uint32_t invalid_psci_func = 0xc400a011;
	uint32_t ret_flag = tftf_get_psci_feature_info(invalid_psci_func);

	if (ret_flag == PSCI_E_NOT_SUPPORTED)
		return TEST_RESULT_SUCCESS;

	tftf_testcase_printf("ERROR: Invalid PSCI function is SUPPORTED\n");

	return TEST_RESULT_FAIL;
}
