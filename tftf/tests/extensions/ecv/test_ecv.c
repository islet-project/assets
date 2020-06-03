/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <test_helpers.h>
#include <tftf.h>
#include <tftf_lib.h>
#include <string.h>

/*
 * TF-A is expected to allow access to CNTPOFF_EL2 register from EL2.
 * Reading this register will trap to EL3 and crash when TF-A has not
 * allowed access.
 */
test_result_t test_ecv_enabled(void)
{
	SKIP_TEST_IF_AARCH32();

#ifdef __aarch64__
	SKIP_TEST_IF_ECV_NOT_SELF_SYNC();
	read_cntpoff_el2();

	return TEST_RESULT_SUCCESS;
#endif	/* __aarch64__ */
}
