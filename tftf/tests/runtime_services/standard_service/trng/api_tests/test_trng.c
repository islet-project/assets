/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
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

	if (version > 0 && version < TRNG_VERSION(1, 0)) {
		return TEST_RESULT_FAIL;
	}
	if (version < 0 && version == TRNG_E_NOT_SUPPORTED) {
		return TEST_RESULT_SKIPPED;
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Verify that TRNG reports implemented functions.
 *
 * Check that TRNG Fetures reports that the TRNG extension:
 *  - reports implementing functions that we know for sure exist, such as TRNG
 *    Features itself
 *  - reports not implementing functions that are part of other standards
 */
test_result_t test_trng_features(void)
{
	int32_t version = tftf_trng_version();

	if (version < 0 && version == TRNG_E_NOT_SUPPORTED) {
		return TEST_RESULT_SKIPPED;
	}
	if (!tftf_trng_feature_implemented(SMC_TRNG_FEATURES)) {
		return TEST_RESULT_FAIL;
	}
	if (tftf_trng_feature_implemented(SMC_PSCI_SYSTEM_RESET)) {
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ TRNG_RND Meets the Zero-fill requirements of the spec
 */
#ifndef __aarch64__
test_result_t test_trng_rnd(void)
{
	int32_t version = tftf_trng_version();
	smc_ret_values rnd_out;

	if (version < 0 && version == TRNG_E_NOT_SUPPORTED) {
		return TEST_RESULT_SKIPPED;
	}
	if (!tftf_trng_feature_implemented(SMC_TRNG_RND)) {
		return TEST_RESULT_SKIPPED;
	}
	rnd_out = tftf_trng_rnd(U(0));
	if (rnd_out.ret0 != TRNG_E_INVALID_PARAMS) {
		ERROR("RND 0 returned 0x%x\n", (uint32_t) rnd_out.ret0);
		return TEST_RESULT_FAIL;
	}
	rnd_out = tftf_trng_rnd(U(1024));
	if (rnd_out.ret0 != TRNG_E_INVALID_PARAMS) {
		ERROR("RND 1024 returned 0x%x\n", (uint32_t) rnd_out.ret0);
		return TEST_RESULT_FAIL;
	}
	rnd_out = tftf_trng_rnd(U(1));
	if (rnd_out.ret0 == TRNG_E_NO_ENTOPY) {
		WARN("There is not a single bit of entropy\n");
		return TEST_RESULT_SKIPPED;
	}
	if ((rnd_out.ret1 & 0xFFFFFFFF) != 0) {
		ERROR("non-zero w1 value 0x%x\n", (uint32_t) rnd_out.ret1);
		return TEST_RESULT_FAIL;
	}
	if ((rnd_out.ret2 & 0xFFFFFFFF) != 0) {
		ERROR("non-zero w2 value 0x%x\n", (uint32_t) rnd_out.ret2);
		return TEST_RESULT_FAIL;
	}
	if ((rnd_out.ret3 & 0xFFFFFFFE) != 0) {
	/* Note: there's an "E" here ^, because we asked for the least
	 * significant bit to be random by passing a "1" to the RND function
	 */
		ERROR("Unexpected w3 value 0x%x\n", (uint32_t) rnd_out.ret3);
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}

#else
test_result_t test_trng_rnd(void)
{
	int32_t version = tftf_trng_version();
	smc_ret_values rnd_out;

	if (version < 0 && version == TRNG_E_NOT_SUPPORTED) {
		return TEST_RESULT_SKIPPED;
	}
	if (!tftf_trng_feature_implemented(SMC_TRNG_RND)) {
		return TEST_RESULT_SKIPPED;
	}
	rnd_out = tftf_trng_rnd(U(0));
	if (rnd_out.ret0 != TRNG_E_INVALID_PARAMS) {
		ERROR("RND 0 returned 0x%lx\n", rnd_out.ret0);
		return TEST_RESULT_FAIL;
	}
	rnd_out = tftf_trng_rnd(U(1024));
	if (rnd_out.ret0 != TRNG_E_INVALID_PARAMS) {
		ERROR("RND 1024 returned 0x%lx\n", rnd_out.ret0);
		return TEST_RESULT_FAIL;
	}
	rnd_out = tftf_trng_rnd(U(1));
	if (rnd_out.ret0 == TRNG_E_NO_ENTOPY) {
		WARN("There is not a single bit of entropy\n");
		return TEST_RESULT_SKIPPED;
	}
	if ((rnd_out.ret1 & 0xFFFFFFFFFFFFFFFF) != 0) {
		ERROR("non-zero x1 value 0x%lx\n", rnd_out.ret1);
		return TEST_RESULT_FAIL;
	}
	if ((rnd_out.ret2 & 0xFFFFFFFFFFFFFFFF) != 0) {
		ERROR("non-zero x2 value 0x%lx\n", rnd_out.ret2);
		return TEST_RESULT_FAIL;
	}
	if ((rnd_out.ret3 & 0xFFFFFFFFFFFFFFFE) != 0) {
		ERROR("Unexpected x3 value 0x%lx\n", rnd_out.ret3);
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}
#endif
