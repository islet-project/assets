/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pauth.h>
#include <psci.h>
#include <smccc.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <tftf.h>
#include <tsp.h>
#include <string.h>

#ifdef __aarch64__

/* Number of ARMv8.3-PAuth keys */
#define NUM_KEYS	5

static const char * const key_name[] = {"IA", "IB", "DA", "DB", "GA"};

static uint128_t pauth_keys_before[NUM_KEYS];
static uint128_t pauth_keys_after[NUM_KEYS];

/* Check if ARMv8.3-PAuth key is enabled */
static bool is_pauth_key_enabled(uint64_t key_bit)
{
	return (IS_IN_EL2() ?
		((read_sctlr_el2() & key_bit) != 0U) :
		((read_sctlr_el1() & key_bit) != 0U));
}

static test_result_t compare_pauth_keys(void)
{
	test_result_t result = TEST_RESULT_SUCCESS;

	for (unsigned int i = 0; i < NUM_KEYS; ++i) {
		if (pauth_keys_before[i] != pauth_keys_after[i]) {
			ERROR("AP%sKey_EL1 read 0x%llx:%llx "
			"expected 0x%llx:%llx\n", key_name[i],
			(uint64_t)(pauth_keys_after[i] >> 64),
			(uint64_t)(pauth_keys_after[i]),
			(uint64_t)(pauth_keys_before[i] >> 64),
			(uint64_t)(pauth_keys_before[i]));

			result = TEST_RESULT_FAIL;
		}
	}
	return result;
}

/*
 * Program or read ARMv8.3-PAuth keys (if already enabled)
 * and store them in <pauth_keys_before> buffer
 */
static void set_store_pauth_keys(void)
{
	uint128_t plat_key;

	memset(pauth_keys_before, 0, NUM_KEYS * sizeof(uint128_t));

	if (is_armv8_3_pauth_apa_api_present()) {
		if (is_pauth_key_enabled(SCTLR_EnIA_BIT)) {
			/* Read APIAKey_EL1 */
			plat_key = read_apiakeylo_el1() |
				((uint128_t)(read_apiakeyhi_el1()) << 64);
			INFO("EnIA is set\n");
		} else {
			/* Program APIAKey_EL1 */
			plat_key = init_apkey();
			write_apiakeylo_el1((uint64_t)plat_key);
			write_apiakeyhi_el1((uint64_t)(plat_key >> 64));
		}
		pauth_keys_before[0] = plat_key;

		if (is_pauth_key_enabled(SCTLR_EnIB_BIT)) {
			/* Read APIBKey_EL1 */
			plat_key = read_apibkeylo_el1() |
				((uint128_t)(read_apibkeyhi_el1()) << 64);
			INFO("EnIB is set\n");
		} else {
			/* Program APIBKey_EL1 */
			plat_key = init_apkey();
			write_apibkeylo_el1((uint64_t)plat_key);
			write_apibkeyhi_el1((uint64_t)(plat_key >> 64));
		}
		pauth_keys_before[1] = plat_key;

		if (is_pauth_key_enabled(SCTLR_EnDA_BIT)) {
			/* Read APDAKey_EL1 */
			plat_key = read_apdakeylo_el1() |
				((uint128_t)(read_apdakeyhi_el1()) << 64);
			INFO("EnDA is set\n");
		} else {
			/* Program APDAKey_EL1 */
			plat_key = init_apkey();
			write_apdakeylo_el1((uint64_t)plat_key);
			write_apdakeyhi_el1((uint64_t)(plat_key >> 64));
		}
		pauth_keys_before[2] = plat_key;

		if (is_pauth_key_enabled(SCTLR_EnDB_BIT)) {
			/* Read APDBKey_EL1 */
			plat_key = read_apdbkeylo_el1() |
				((uint128_t)(read_apdbkeyhi_el1()) << 64);
			INFO("EnDB is set\n");
		} else {
			/* Program APDBKey_EL1 */
			plat_key = init_apkey();
			write_apdbkeylo_el1((uint64_t)plat_key);
			write_apdbkeyhi_el1((uint64_t)(plat_key >> 64));
		}
		pauth_keys_before[3] = plat_key;
	}

	/*
	 * It is safe to assume that Generic Pointer authentication code key
	 * APGAKey_EL1 can be re-programmed, as this key is not set in
	 * TF-A Test suite and PACGA instruction is not used.
	 */
	if (is_armv8_3_pauth_gpa_gpi_present()) {
		/* Program APGAKey_EL1 */
		plat_key = init_apkey();
		write_apgakeylo_el1((uint64_t)plat_key);
		write_apgakeyhi_el1((uint64_t)(plat_key >> 64));
		pauth_keys_before[4] = plat_key;
	}

	isb();
}

/*
 * Read ARMv8.3-PAuth keys and store them in
 * <pauth_keys_after> buffer
 */
static void read_pauth_keys(void)
{
	memset(pauth_keys_after, 0, NUM_KEYS * sizeof(uint128_t));

	if (is_armv8_3_pauth_apa_api_present()) {
		/* Read APIAKey_EL1 */
		pauth_keys_after[0] = read_apiakeylo_el1() |
			((uint128_t)(read_apiakeyhi_el1()) << 64);

		/* Read APIBKey_EL1 */
		pauth_keys_after[1] = read_apibkeylo_el1() |
			((uint128_t)(read_apibkeyhi_el1()) << 64);

		/* Read APDAKey_EL1 */
		pauth_keys_after[2] = read_apdakeylo_el1() |
			((uint128_t)(read_apdakeyhi_el1()) << 64);

		/* Read APDBKey_EL1 */
		pauth_keys_after[3] = read_apdbkeylo_el1() |
			((uint128_t)(read_apdbkeyhi_el1()) << 64);
	}

	if (is_armv8_3_pauth_gpa_gpi_present()) {
		/* Read APGAKey_EL1 */
		pauth_keys_after[4] = read_apgakeylo_el1() |
			((uint128_t)(read_apgakeyhi_el1()) << 64);
	}
}
#endif	/* __aarch64__ */

/*
 * TF-A is expected to allow access to key registers from lower EL's,
 * reading the keys excercises this, on failure this will trap to
 * EL3 and crash.
 */
test_result_t test_pauth_reg_access(void)
{
	SKIP_TEST_IF_AARCH32();
#ifdef __aarch64__
	SKIP_TEST_IF_PAUTH_NOT_SUPPORTED();
	read_pauth_keys();
	return TEST_RESULT_SUCCESS;
#endif	/* __aarch64__ */
}

/*
 * Makes a call to PSCI version, and checks that the EL3 pauth keys are not
 * leaked when it returns
 */
test_result_t test_pauth_leakage(void)
{
	SKIP_TEST_IF_AARCH32();
#ifdef __aarch64__
	SKIP_TEST_IF_PAUTH_NOT_SUPPORTED();
	set_store_pauth_keys();

	tftf_get_psci_version();

	read_pauth_keys();

	return compare_pauth_keys();
#endif	/* __aarch64__ */
}

/* Test execution of ARMv8.3-PAuth instructions */
test_result_t test_pauth_instructions(void)
{
	SKIP_TEST_IF_AARCH32();
#ifdef __aarch64__
	SKIP_TEST_IF_PAUTH_NOT_SUPPORTED();

#if ARM_ARCH_AT_LEAST(8, 3)
	/* Pointer authentication instructions */
	__asm__ volatile (
		"paciasp\n"
		"autiasp\n"
		"paciasp\n"
		"xpaclri"
	);
	return TEST_RESULT_SUCCESS;
#else
	tftf_testcase_printf("Pointer Authentication instructions "
				"are not supported on ARMv%u.%u\n",
				ARM_ARCH_MAJOR, ARM_ARCH_MINOR);
	return TEST_RESULT_SKIPPED;
#endif	/* ARM_ARCH_AT_LEAST(8, 3) */

#endif	/* __aarch64__ */
}

/*
 * Makes a call to TSP ADD, and checks that the checks that the Secure World
 * pauth keys are not leaked
 */
test_result_t test_pauth_leakage_tsp(void)
{
	SKIP_TEST_IF_AARCH32();
#ifdef __aarch64__
	smc_args tsp_svc_params;
	smc_ret_values tsp_result = {0};

	SKIP_TEST_IF_PAUTH_NOT_SUPPORTED();
	SKIP_TEST_IF_TSP_NOT_PRESENT();

	set_store_pauth_keys();

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
		tftf_testcase_printf("TSP add returned wrong result: "
				     "got %d %d %d expected: 0 8 12\n",
				     (unsigned int)tsp_result.ret0,
				     (unsigned int)tsp_result.ret1,
				     (unsigned int)tsp_result.ret2);
		return TEST_RESULT_FAIL;
	}

	read_pauth_keys();

	return compare_pauth_keys();
#endif	/* __aarch64__ */
}
