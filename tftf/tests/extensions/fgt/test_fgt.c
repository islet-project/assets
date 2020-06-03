/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <test_helpers.h>
#include <tftf_lib.h>
#include <tftf.h>
#include <string.h>
#include <arch_helpers.h>

/*
 * TF-A is expected to allow access to ARMv8.6-FGT system registers from EL2.
 * Reading these registers causes a trap to EL3 and crash when TF-A has not
 * allowed access.
 */
test_result_t test_fgt_enabled(void)
{
	SKIP_TEST_IF_AARCH32();

#ifdef __aarch64__
	SKIP_TEST_IF_FGT_NOT_SUPPORTED();
	read_hfgrtr_el2();
	read_hfgwtr_el2();
	read_hfgitr_el2();
	read_hdfgrtr_el2();
	read_hdfgwtr_el2();

	return TEST_RESULT_SUCCESS;
#endif	/* __aarch64__ */
}
