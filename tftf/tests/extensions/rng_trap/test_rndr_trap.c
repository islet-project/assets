/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <arch_features.h>
#include <test_helpers.h>
#include <tftf.h>
#include <tftf_lib.h>

/*
 * This very simple test just ensures that a RNDR read access causes a trap
 * to EL3.
 */
test_result_t test_rndr_trap_enabled(void)
{
#if defined __aarch64__
	/* Make sure FEAT_RNG_TRAP is supported. */
	SKIP_TEST_IF_RNG_TRAP_NOT_SUPPORTED();

	/* Attempt to read RNDR. */
	read_rndr();

	/*
	 * If we make it this far, the test fails, as there was no trap
	 * to EL3 triggered.
	 */
	return TEST_RESULT_FAIL;
#else
	/* Skip test if AArch32 */
	SKIP_TEST_IF_AARCH32();
#endif
}
