/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <tftf.h>
#include <tftf_lib.h>
#include <tsp.h>
#include <test_helpers.h>

test_result_t test_mte_instructions(void)
{
	SKIP_TEST_IF_AARCH32();
#ifdef AARCH64
	SKIP_TEST_IF_MTE_SUPPORT_LESS_THAN(MTE_IMPLEMENTED_EL0);

	/* irg */
	__asm__ volatile (".inst 0xD29BD5A9");
	__asm__ volatile (".inst 0x9ADF1129");
	/* addg */
	__asm__ volatile (".inst 0x91800129");
	/* subg */
	__asm__ volatile (".inst 0xD1800129");

	return TEST_RESULT_SUCCESS;
#endif /* AARCH64 */
}

test_result_t test_mte_leakage(void)
{
	SKIP_TEST_IF_AARCH32();
#ifdef AARCH64
	smc_args tsp_svc_params;
	int gcr_el1;

	SKIP_TEST_IF_MTE_SUPPORT_LESS_THAN(MTE_IMPLEMENTED_ELX);
	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/* We only test gcr_el1 as writes to other MTE registers are ignored */
	write_gcr_el1(0xdd);

	/* Standard SMC to ADD two numbers */
	tsp_svc_params.fid = TSP_STD_FID(TSP_ADD);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	tftf_smc(&tsp_svc_params);

	gcr_el1 = read_gcr_el1();
	if (gcr_el1 != 0xdd) {
		printf("gcr_el1 has changed to %d\n", gcr_el1);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
#endif /* AARCH64 */
}
