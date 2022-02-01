/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <test_helpers.h>
#include <tftf_lib.h>
#include <tftf.h>

/*
 * EL3 is expected to allow access to branch record buffer control registers
 * from NS world. Accessing these registers will trap to EL3 and crash when EL3
 * has not properly enabled it.
 */
test_result_t test_brbe_enabled(void)
{
	SKIP_TEST_IF_AARCH32();

#ifdef __aarch64__
	SKIP_TEST_IF_BRBE_NOT_SUPPORTED();

	read_brbcr_el1();
	read_brbcr_el2();
	read_brbfcr_el1();
	read_brbts_el1();
	read_brbinfinj_el1();
	read_brbsrcinj_el1();
	read_brbtgtinj_el1();
	read_brbidr0_el1();

	return TEST_RESULT_SUCCESS;
#endif /* __aarch64__ */
}
