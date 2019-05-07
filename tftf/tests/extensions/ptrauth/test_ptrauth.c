/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psci.h>
#include <smccc.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <tftf.h>
#include <tsp.h>
#include <string.h>

/* The length of the array used to hold the pauth keys */
#define LENGTH_KEYS 10

#ifndef AARCH32
static void read_pauth_keys(uint64_t *pauth_keys, unsigned int len)
{
	assert(len >= LENGTH_KEYS);

	memset(pauth_keys, 0, len * sizeof(uint64_t));

	if (is_armv8_3_pauth_apa_api_present()) {
		/* read instruction keys a and b (both 128 bit) */
		pauth_keys[0] = read_apiakeylo_el1();
		pauth_keys[1] = read_apiakeyhi_el1();

		pauth_keys[2] = read_apibkeylo_el1();
		pauth_keys[3] = read_apibkeyhi_el1();

		/* read data keys a and b (both 128 bit) */
		pauth_keys[4] = read_apdakeylo_el1();
		pauth_keys[5] = read_apdakeyhi_el1();

		pauth_keys[6] = read_apdbkeylo_el1();
		pauth_keys[7] = read_apdbkeyhi_el1();
	}

	if (is_armv8_3_pauth_gpa_gpi_present()) {
		/* read generic key */
		pauth_keys[8] = read_apgakeylo_el1();
		pauth_keys[9] = read_apgakeyhi_el1();
	}

}
#endif

/*
 * TF-A is expected to allow access to key registers from lower EL's,
 * reading the keys excercises this, on failure this will trap to
 * EL3 and crash.
 */
test_result_t test_pauth_reg_access(void)
{
	SKIP_TEST_IF_AARCH32();
#ifndef AARCH32
	uint64_t pauth_keys[LENGTH_KEYS];
	SKIP_TEST_IF_PAUTH_NOT_SUPPORTED();
	read_pauth_keys(pauth_keys, LENGTH_KEYS);
	return TEST_RESULT_SUCCESS;
#endif
}

/*
 * Makes a call to psci version, and checks that the EL3 pauth keys are not
 * leaked when it returns
 */
#if (__GNUC__ > 7 || (__GNUC__ == 7 && __GNUC_MINOR__ > 0)) && AARCH64
__attribute__((target("sign-return-address=all")))
#endif
test_result_t test_pauth_leakage(void)
{
	SKIP_TEST_IF_AARCH32();
#ifndef AARCH32
	uint64_t pauth_keys_before[LENGTH_KEYS];
	uint64_t pauth_keys_after[LENGTH_KEYS];
	SKIP_TEST_IF_PAUTH_NOT_SUPPORTED();

	read_pauth_keys(pauth_keys_before, LENGTH_KEYS);

	tftf_get_psci_version();

	read_pauth_keys(pauth_keys_after, LENGTH_KEYS);

	if (memcmp(pauth_keys_before, pauth_keys_after,
			LENGTH_KEYS * sizeof(uint64_t)) != 0)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
#endif
}

/* Uses the pauth instructions, this checks the enable PAUTH bit has been set */
test_result_t test_pauth_instructions(void)
{
	SKIP_TEST_IF_AARCH32();
#ifndef AARCH32
	SKIP_TEST_IF_PAUTH_NOT_SUPPORTED();
	/*
	 * Pointer authentication instructions (explicit encoding for compilers
	 * that do not recognize these instructions)
	 */
	/* paciasp */
	__asm__ volatile (".inst 0xD503233F");
	/* autiasp */
	__asm__ volatile (".inst 0xD50323BF");
	/* paciasp */
	__asm__ volatile (".inst 0xD503233F");
	/* xpaclri */
	__asm__ volatile (".inst 0xD50320FF");
	return TEST_RESULT_SUCCESS;
#endif
}

/*
 * Makes a call to TSP ADD, and checks that the checks that the Secure World
 * pauth keys are not leaked
 */
#if (__GNUC__ > 7 || (__GNUC__ == 7 && __GNUC_MINOR__ > 0)) && AARCH64
__attribute__((target("sign-return-address=all")))
#endif
test_result_t test_pauth_leakage_tsp(void)
{
	SKIP_TEST_IF_AARCH32();
#ifndef AARCH32
	smc_args tsp_svc_params;
	smc_ret_values tsp_result = {0};
	uint64_t pauth_keys_before[LENGTH_KEYS];
	uint64_t pauth_keys_after[LENGTH_KEYS];

	SKIP_TEST_IF_PAUTH_NOT_SUPPORTED();
	SKIP_TEST_IF_TSP_NOT_PRESENT();

	read_pauth_keys(pauth_keys_before, LENGTH_KEYS);

	/* Standard SMC to ADD two numbers */
	tsp_svc_params.fid = TSP_STD_FID(TSP_ADD);
	tsp_svc_params.arg1 = 4;
	tsp_svc_params.arg2 = 6;
	tsp_result = tftf_smc(&tsp_svc_params);

	/*
	 * Check the result of the addition-TSP_ADD will add
	 * the arguments to themselves and return
	 */
	if (tsp_result.ret0 != 0 || tsp_result.ret1 != 8 ||
				tsp_result.ret2 != 12) {
		tftf_testcase_printf("TSP add returned wrong result:"
				     "got %d %d %d expected: 0 8 12\n",
						(unsigned int)tsp_result.ret0,
						(unsigned int)tsp_result.ret1,
						(unsigned int)tsp_result.ret2);

		return TEST_RESULT_FAIL;
	}
	read_pauth_keys(pauth_keys_after, LENGTH_KEYS);

	if (memcmp(pauth_keys_before, pauth_keys_after,
			LENGTH_KEYS * sizeof(uint64_t)) != 0)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
#endif
}
