/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psci.h>
#include <smccc.h>
#include <tftf_lib.h>

/*
 * @Test_Aim@ Validate the SYSTEM_RESET call.
 * Test SUCCESS in case of system reset.
 * Test FAIL in case of execution not terminated.
 */
test_result_t test_system_reset(void)
{
	smc_args args = { SMC_PSCI_SYSTEM_RESET };

	if (tftf_is_rebooted() == 1) {
		/* Successfully resumed from SYSTEM_RESET */
		return TEST_RESULT_SUCCESS;
	}

	tftf_notify_reboot();
	tftf_smc(&args);

	/* The PSCI SYSTEM_RESET call is not supposed to return */
	tftf_testcase_printf("System didn't reboot properly\n");

	return TEST_RESULT_FAIL;
}
