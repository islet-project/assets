/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <test_helpers.h>
#include <tftf_lib.h>
#include <tftf.h>

/*
 * EL3 is expected to allow access to trace filter control registers from EL2.
 * Reading these register will trap to EL3 and crash when EL3 has not
 * allowed access.
 */
test_result_t test_trf_enabled(void)
{
	SKIP_TEST_IF_TRF_NOT_SUPPORTED();

#ifdef __aarch64__
	read_trfcr_el1();
	read_trfcr_el2();
#else
	read_htrfcr();
	read_trfcr();
#endif /* __aarch64__ */

	return TEST_RESULT_SUCCESS;
}
