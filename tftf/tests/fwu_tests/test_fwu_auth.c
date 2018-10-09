/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <fwu_nvm.h>
#include <io_storage.h>
#include <platform.h>
#include <platform_def.h>
#include <psci.h>
#include <smccc.h>
#include <status.h>
#include <tftf_lib.h>

/*
 * @Test_Aim@ Validate the FWU AUTH failure case.
 * The Firmware Update feature implemented in Trusted Firmware-A
 * code needs to be tested to check if FWU process gets started
 * when watchdog resets the system due to Authentication
 * failure of an image in BL1/BL2 stage.
 * Test SUCCESS in case Firmware Update was done.
 * Test FAIL in case Firmware Update was not done.
 */
test_result_t test_fwu_auth(void)
{
	STATUS status;
	unsigned int flag;
	smc_args args = { SMC_PSCI_SYSTEM_RESET };
	smc_ret_values ret = {0};

	if (tftf_is_rebooted()) {
		/*
		 * Check if the FIP update is done.
		 */
		status = fwu_nvm_read(FWU_TFTF_TESTCASE_BUFFER_OFFSET,	&flag, 4);
		if (status != STATUS_SUCCESS) {
			tftf_testcase_printf("Failed to read NVM (%d)\n", status);
			return TEST_RESULT_FAIL;
		}

		if (flag != FIP_IMAGE_UPDATE_DONE_FLAG) {
			tftf_testcase_printf("FIP was not updated\n");
			return TEST_RESULT_FAIL;
		}

		return TEST_RESULT_SUCCESS;
	}

	/*
	 * Corrupt the flash offset for authentication failure.
	 */
	flag = 0xdeadbeef;
	status = fwu_nvm_write(FIP_CORRUPT_OFFSET, &flag, 4);
	if (status != STATUS_SUCCESS) {
		tftf_testcase_printf("Failed to corrupt FIP (%d)\n", status);
		return TEST_RESULT_SKIPPED;
	}

	/*
	 * Provide the backup FIP address.
	 */
	flag = FIP_BKP_ADDRESS;
	status = fwu_nvm_write(FWU_TFTF_TESTCASE_BUFFER_OFFSET, &flag, 4);
	if (status != STATUS_SUCCESS) {
		tftf_testcase_printf("Failed to update backup FIP address (%d)\n",
				status);
		return TEST_RESULT_SKIPPED;
	}

	/* Notify that we are rebooting now. */
	tftf_notify_reboot();

	/* Request PSCI system reset. */
	ret = tftf_smc(&args);

	/* The PSCI SYSTEM_RESET call is not supposed to return */
	tftf_testcase_printf("System didn't reboot properly (%d)\n",
			(unsigned int)ret.ret0);

	return TEST_RESULT_FAIL;
}
