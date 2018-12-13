/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <smccc.h>
#include <test_helpers.h>
#include <tftf_lib.h>

/**
 * @Test_Aim@ test_smc_tsp_std_fns_call - Query standard function information
 *                                        against TrustedOS service calls.
 *
 * This test targets the TSP, i.e. the Trusted Firmware-A Test Secure-EL1
 * Payload. If there is no Trusted OS in the software stack, or if it is not
 * the TSP, this test will be skipped.
 *
 * The following queries are performed:
 * 1) UID
 * 2) Call count
 * 3) Call revision info
 */
test_result_t test_smc_tsp_std_fns_call(void)
{
	smc_args std_svc_args;
	smc_ret_values ret;

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/* TrustedOS Service Call Count */
	std_svc_args.fid = SMC_TOS_CALL_COUNT;
	ret = tftf_smc(&std_svc_args);
	if (ret.ret0 != TSP_NUM_FID) {
		tftf_testcase_printf("Wrong Call Count: expected %u,\n"
				     " got %llu\n", TSP_NUM_FID,
				     (unsigned long long)ret.ret0);
		return TEST_RESULT_FAIL;
	}

	/* TrustedOS Service Call Revision details */
	std_svc_args.fid = SMC_TOS_REVISION;
	ret = tftf_smc(&std_svc_args);
	if ((ret.ret0 != TSP_REVISION_MAJOR) ||
		 ret.ret1 != TSP_REVISION_MINOR) {
		tftf_testcase_printf("Wrong Revision: expected {%u.%u}\n"
				     "                     got {%llu.%llu}\n",
				     TSP_REVISION_MAJOR, TSP_REVISION_MINOR,
				     (unsigned long long)ret.ret0,
				     (unsigned long long)ret.ret1);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
