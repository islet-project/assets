/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psci.h>
#include <smccc.h>
#include <tftf_lib.h>

/*
 * @Test_Aim@ Validate the SYSTEM_OFF call.
 * Test SUCCESS in case of system shutdown.
 * Test FAIL in case of execution not terminated.
 */
test_result_t test_system_off(void)
{
	smc_args args = { SMC_PSCI_SYSTEM_OFF };

	if (tftf_is_rebooted()) {
		/* Successfully resumed from system off */
		return TEST_RESULT_SUCCESS;
	}

	tftf_notify_reboot();
	tftf_smc(&args);

	/* The PSCI SYSTEM_OFF call is not supposed to return */
	tftf_testcase_printf("System didn't shutdown properly\n");
	return TEST_RESULT_FAIL;
}
