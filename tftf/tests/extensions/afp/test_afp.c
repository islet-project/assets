/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <test_helpers.h>

test_result_t test_afp_support(void)
{
	SKIP_TEST_IF_AARCH32();

#ifdef __aarch64__
	test_result_t ret;
	uint64_t saved_fpcr, fpcr;

	SKIP_TEST_IF_AFP_NOT_SUPPORTED();

	saved_fpcr = read_fpcr();
	/* Write advanced floating point controlling bits */
	write_fpcr(saved_fpcr | FPCR_FIZ_BIT | FPCR_AH_BIT | FPCR_NEP_BIT);

	fpcr = read_fpcr();
	/* Check if all bits got written successfully */
	if ((fpcr | ~(FPCR_FIZ_BIT | FPCR_AH_BIT | FPCR_NEP_BIT)) == ~0ULL) {
		ret = TEST_RESULT_SUCCESS;
	} else {
		ret = TEST_RESULT_FAIL;
	}

	write_fpcr(saved_fpcr);

	return ret;
#endif /* __aarch64__ */
}
