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
 * EL3 is expected to allow access to trace control registers from EL2.
 * Reading these register will trap to EL3 and crash when EL3 has not
 * allowed access.
 */
test_result_t test_trbe_enabled(void)
{
        SKIP_TEST_IF_AARCH32();

#ifdef __aarch64__
	SKIP_TEST_IF_TRBE_NOT_SUPPORTED();
	read_trblimitr_el1();
	read_trbptr_el1();
	read_trbbaser_el1();
	read_trbsr_el1();
	read_trbmar_el1();
	read_trbtrg_el1();
	read_trbidr_el1();
	return TEST_RESULT_SUCCESS;
#endif  /* __aarch64__ */
}
