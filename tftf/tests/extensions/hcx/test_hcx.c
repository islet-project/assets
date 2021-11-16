/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <tftf_lib.h>
#include <tftf.h>
#include <arch_helpers.h>
#include <arch_features.h>

/* This very simple test just ensures that HCRX_EL2 access does not trap. */
test_result_t test_feat_hcx_enabled(void)
{
#ifdef __aarch64__
	/* Make sure FEAT_HCX is supported. */
	if (!get_feat_hcx_support()) {
		return TEST_RESULT_SKIPPED;
	}

	/* Attempt to read HCRX_EL2, if not enabled this should trap to EL3. */
	read_hcrx_el2();

	/* If we make it this far, the test was successful. */
	return TEST_RESULT_SUCCESS;
#else
	/* Skip test if AArch32 */
	return TEST_RESULT_SKIPPED;
#endif
}
