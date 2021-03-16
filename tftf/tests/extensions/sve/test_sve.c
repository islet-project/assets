/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_features.h>
#include <arch_helpers.h>
#include <debug.h>
#include <stdlib.h>
#include <test_helpers.h>
#include <tftf_lib.h>

#include "./test_sve.h"

#if __GNUC__ > 8 || (__GNUC__ == 8 && __GNUC_MINOR__ > 0)

extern void sve_subtract_arrays(int *difference, const int *sve_op_1,
				const int *sve_op_2);

static int sve_difference[SVE_ARRAYSIZE];
static int sve_op_1[SVE_ARRAYSIZE];
static int sve_op_2[SVE_ARRAYSIZE];

/*
 * @Test_Aim@ Test SVE support when the extension is enabled.
 *
 * Execute some SVE instructions. These should not be trapped to EL3, as TF-A is
 * responsible for enabling SVE for Non-secure world.
 *
 * If they are trapped, we won't recover from that and the test session will
 * effectively be aborted.
 */
test_result_t test_sve_support(void)
{
	SKIP_TEST_IF_SVE_NOT_SUPPORTED();

	for (int i = 0; i < SVE_ARRAYSIZE; i++) {
		/* Generate a random number between 200 and 299 */
		sve_op_1[i] = (rand() % 100) + 200;
		/* Generate a random number between 0 and 99 */
		sve_op_2[i] = rand() % 100;
	}

	/* Perform SVE operations */
	sve_subtract_arrays(sve_difference, sve_op_1, sve_op_2);

	return TEST_RESULT_SUCCESS;
}

#else

test_result_t test_sve_support(void)
{
	tftf_testcase_printf("Unsupported compiler\n");
	return TEST_RESULT_SKIPPED;
}

#endif /* __GNUC__ > 8 || (__GNUC__ == 8 && __GNUC_MINOR__ > 0) */
