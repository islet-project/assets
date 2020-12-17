/*
 * Copyright (c) 2019-2020, Arm Limited. All rights reserved.
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
#ifdef __aarch64__
	SKIP_TEST_IF_MTE_SUPPORT_LESS_THAN(MTE_IMPLEMENTED_EL0);

	/*
	 * This code must be compiled with '-march=armv8.5-memtag' option
	 * by setting 'ARM_ARCH_FEATURE=memtag' and 'ARM_ARCH_MINOR=5'
	 * build flags in tftf_config/fvp-cpu-extensions when this CI
	 * configuration is built separately.
	 * Otherwise this compiler's option must be specified explicitly.
	 *
	 * Execute Memory Tagging Extension instructions.
	 */
	__asm__ volatile (
		".arch	armv8.5-a+memtag\n"
		"mov	x0, #0xDEAD\n"
		"irg	x0, x0\n"
		"addg	x0, x0, #0x0, #0x0\n"
		"subg	x0, x0, #0x0, #0x0"
	);

	return TEST_RESULT_SUCCESS;
#endif /* __aarch64__ */
}

test_result_t test_mte_leakage(void)
{
	SKIP_TEST_IF_AARCH32();
#ifdef __aarch64__
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
#endif /* __aarch64__ */
}
