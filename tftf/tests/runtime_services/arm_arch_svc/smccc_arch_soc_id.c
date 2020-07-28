/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <arm_arch_svc.h>
#include <debug.h>
#include <smccc.h>
#include <string.h>
#include <tftf_lib.h>

/*
 * Return SOC ID parameters(SOC revision/SOC version) according
 * to argument passed
 */
static smc_ret_values get_soc_id_param(u_register_t arg)
{
	smc_args args;
	smc_ret_values ret;

	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_ARCH_SOC_ID;
	args.arg1 = arg;
	ret = tftf_smc(&args);

	return ret;
}

/* Entry function to execute SMCCC_ARCH_SOC_ID test */
test_result_t test_smccc_arch_soc_id(void)
{
	smc_args args;
	smc_ret_values ret;
	int32_t expected_ver;

	/* Check if SMCCC version is at least v1.2 */
	expected_ver = MAKE_SMCCC_VERSION(1, 2);
	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_VERSION;
	ret = tftf_smc(&args);
	if ((int32_t)ret.ret0 < expected_ver) {
		tftf_testcase_printf("Unexpected SMCCC version: 0x%x\n",
		       (int)ret.ret0);
		return TEST_RESULT_SKIPPED;
	}

	/* Check if SMCCC_ARCH_SOC_ID is implemented or not */
	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_ARCH_FEATURES;
	args.arg1 = SMCCC_ARCH_SOC_ID;
	ret = tftf_smc(&args);
	if ((int)ret.ret0 == SMC_ARCH_CALL_NOT_SUPPORTED) {
		tftf_testcase_printf("SMCCC_ARCH_SOC_ID is not implemented\n");
		return TEST_RESULT_SKIPPED;
	}

	/* If the call returns SMC_OK then SMCCC_ARCH_SOC_ID is feature available */
	if ((int)ret.ret0 == SMC_OK) {
		ret = get_soc_id_param(SMC_GET_SOC_REVISION);

		if ((int)ret.ret0 == SMC_ARCH_CALL_INVAL_PARAM) {
			tftf_testcase_printf("Invalid param passed to	\
					SMCCC_ARCH_SOC_ID\n");
			return TEST_RESULT_FAIL;
		} else if ((int)ret.ret0 == SMC_ARCH_CALL_NOT_SUPPORTED) {
			tftf_testcase_printf("SOC Rev is not implemented\n");
			return TEST_RESULT_FAIL;
		}

		tftf_testcase_printf("SOC Rev = 0x%x\n", (int)ret.ret0);

		ret = get_soc_id_param(SMC_GET_SOC_VERSION);

		if ((int)ret.ret0 == SMC_ARCH_CALL_INVAL_PARAM) {
			tftf_testcase_printf("Invalid param passed to	\
					SMCCC_ARCH_SOC_ID\n");
			return TEST_RESULT_FAIL;
		} else if ((int)ret.ret0 == SMC_ARCH_CALL_NOT_SUPPORTED) {
			tftf_testcase_printf("SOC Ver is not implemented\n");
			return TEST_RESULT_FAIL;
		}

		tftf_testcase_printf("SOC Ver = 0x%x\n", (int)ret.ret0);

	} else {
		ERROR("Invalid error during SMCCC_ARCH_FEATURES call = 0x%x\n",
			(int)ret.ret0);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
