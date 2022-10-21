/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <platform.h>
#include <psci.h>
#include <smccc.h>
#include <string.h>
#include <errno.h>
#include <tftf_lib.h>
#include <trng.h>

/*
 * @Test_Aim@ exercise TRNG Version SMC.
 *
 * This test should exercise the trng version call. Versions before 1.0 don't
 * exsit, so should fail. This should also skip if TRNG is not implemented.
 */
test_result_t test_trng_version(void)
{
	int32_t version = tftf_trng_version();

	if (version == TRNG_E_NOT_SUPPORTED) {
		return TEST_RESULT_SKIPPED;
	}

	if (version < TRNG_VERSION(1, 0)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Verify that TRNG reports implemented functions.
 *
 * Check that TRNG Features reports that all TRNG functions
 * are implemented.
 */
test_result_t test_trng_features(void)
{
	int32_t version = tftf_trng_version();

	if (version == TRNG_E_NOT_SUPPORTED) {
		return TEST_RESULT_SKIPPED;
	}

	if (!(tftf_trng_feature_implemented(SMC_TRNG_FEATURES) &&
	      tftf_trng_feature_implemented(SMC_TRNG_UUID) &&
	      tftf_trng_feature_implemented(SMC_TRNG_RND))) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ TRNG_RND Meets the Zero-fill requirements of the spec
 */
test_result_t test_trng_rnd(void)
{
	smc_ret_values rnd_out;
	size_t msb_shift = (TRNG_MAX_BITS/3) - U(1);

	int32_t version = tftf_trng_version();

	if (version == TRNG_E_NOT_SUPPORTED) {
		return TEST_RESULT_SKIPPED;
	}

	/* Ensure function is implemented before requesting Entropy */
	if (!(tftf_trng_feature_implemented(SMC_TRNG_RND))) {
		return TEST_RESULT_FAIL;
	}

	/* Test invalid entropy sizes */
	rnd_out = tftf_trng_rnd(U(0));
	if (rnd_out.ret0 != TRNG_E_INVALID_PARAMS) {
		ERROR("RND 0 returned 0x%lx\n", rnd_out.ret0);
		return TEST_RESULT_FAIL;
	}

	rnd_out = tftf_trng_rnd(TRNG_MAX_BITS + U(1));
	if (rnd_out.ret0 != TRNG_E_INVALID_PARAMS) {
		ERROR("RND 0x%x returned 0x%lx\n",  TRNG_MAX_BITS + U(1),
				rnd_out.ret0);
		return TEST_RESULT_FAIL;
	}

	/* Test valid corner cases.
	 * Here we expect the bits in Entropy[MAX_BITS-1:N]
	 * to be 0, where N is the requested number of bits
	 * of entropy */

	/* For N = 1, all returned entropy bits should be 0
	 * except the least significant bit */
	rnd_out = tftf_trng_rnd(U(1));
	if (rnd_out.ret0 == TRNG_E_NO_ENTROPY) {
		WARN("There is not a single bit of entropy\n");
		return TEST_RESULT_SKIPPED;
	}
	if ((rnd_out.ret1 & TRNG_ENTROPY_MASK) != 0) {
		ERROR("non-zero r1 value 0x%lx\n", rnd_out.ret1);
		return TEST_RESULT_FAIL;
	}
	if ((rnd_out.ret2 & TRNG_ENTROPY_MASK) != 0) {
		ERROR("non-zero r2 value 0x%lx\n", rnd_out.ret2);
		return TEST_RESULT_FAIL;
	}
	if ((rnd_out.ret3 & (TRNG_ENTROPY_MASK - U(1))) != 0) {
		ERROR("Unexpected r3 value 0x%lx\n", rnd_out.ret3);
		return TEST_RESULT_FAIL;
	}

	/* For N = MAX_BITS-1, the most significant bit should be 0 */
	rnd_out = tftf_trng_rnd(TRNG_MAX_BITS - U(1));
	if (rnd_out.ret0 == TRNG_E_NO_ENTROPY) {
		WARN("There is not a single bit of entropy\n");
		return TEST_RESULT_SKIPPED;
	}
	if ((rnd_out.ret1 & (1 << msb_shift)) != 0) {
		ERROR("Unexpected r1 value 0x%lx\n", rnd_out.ret1);
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}
