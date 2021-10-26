/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <test_helpers.h>

/*
 * Test that the PSTATE bits not set in Aarch64.TakeException but
 * set to a default when taking an exception to EL3 are maintained
 * after an exception and that changes in TSP do not effect the PSTATE
 * in TFTF and vice versa.
 */
test_result_t tsp_check_pstate_maintained_on_exception(void)
{
	smc_args tsp_svc_params;
	smc_ret_values ret;
	u_register_t dit;
	u_register_t dit_bit;

	SKIP_TEST_IF_TSP_NOT_PRESENT();
	SKIP_TEST_IF_DIT_NOT_SUPPORTED();

#ifdef __aarch64__
	dit_bit = DIT_BIT;
#else
	dit_bit = CPSR_DIT_BIT;
#endif

	write_dit(dit_bit);

	/* Standard SMC */
	tsp_svc_params.fid = TSP_STD_FID(TSP_CHECK_DIT);
	tsp_svc_params.arg1 = 0;
	tsp_svc_params.arg2 = 0;
	ret = tftf_smc(&tsp_svc_params);
	if (ret.ret1 == 0) {
		if (ret.ret2 == 0xffff) {
			tftf_testcase_printf("DIT bit not supported by TSP");
			return TEST_RESULT_SKIPPED;
		}
		tftf_testcase_printf("DIT bit in the TSP is not 0.\n");
		return TEST_RESULT_FAIL;
	}

	dit = read_dit();
	if (dit != dit_bit) {
		tftf_testcase_printf("DIT bit in TFTF was not maintained.\n"
				     "Expected: 0x%x, Actual: 0x%x",
				     (uint32_t) dit_bit, (uint32_t) dit);
		return TEST_RESULT_FAIL;
	}

	tsp_svc_params.fid = TSP_STD_FID(TSP_CHECK_DIT);
	tsp_svc_params.arg1 = dit_bit;
	tsp_svc_params.arg2 = 0;
	ret = tftf_smc(&tsp_svc_params);
	if (ret.ret1 == 0) {
		tftf_testcase_printf("DIT bit in the TSP was not maintained\n"
				     "Expected: 0x%x, Actual: 0x%x",
				     (uint32_t) dit_bit, (uint32_t) ret.ret2);
		return TEST_RESULT_FAIL;
	}

	dit = read_dit();
	if (dit != dit_bit) {
		tftf_testcase_printf("DIT bit in TFTF was not maintained.\n"
				     "Expected: 0x%x, Actual: 0x%x",
				     (uint32_t) dit_bit, (uint32_t) dit);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
