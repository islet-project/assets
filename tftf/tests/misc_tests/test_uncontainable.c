/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <tftf_lib.h>

#ifndef AARCH32

extern void inject_uncontainable(void);

test_result_t test_uncontainable(void)
{
	inject_uncontainable();

	return TEST_RESULT_SUCCESS;
}

#else

test_result_t test_uncontainable(void)
{
	tftf_testcase_printf("Not supported on AArch32.\n");
	return TEST_RESULT_SKIPPED;
}

#endif
