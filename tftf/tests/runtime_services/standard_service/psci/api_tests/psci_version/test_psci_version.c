/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psci.h>
#include <smccc.h>
#include <tftf_lib.h>

/*
 * @Test_Aim@ Check the version of PSCI implemented
 *
 * This test relies on the PSCI_VERSION SMC call.
 * It expects versions  0.1, 0.2, 1.0, 1.1
 */
test_result_t test_psci_version(void)
{
	int version;

	version = tftf_get_psci_version();
	if (!tftf_is_valid_psci_version(version)) {
		tftf_testcase_printf(
			"Wrong PSCI version:0x%08x\n", version);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
