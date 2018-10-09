/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <firmware_image_package.h>
#include <fwu_nvm.h>
#include <io_storage.h>
#include <platform.h>
#include <psci.h>
#include <smccc.h>
#include <status.h>
#include <tftf_lib.h>

/*
 * @Test_Aim@ Validate the FWU ToC invalid case.
 * The Firmware Update feature implemented in Trusted Firmware-A
 * code needs to be tested to check if FWU process gets started
 * or not when the ToC header value in fip.bin is invalid.
 * Test SUCCESS in case ToC is found valid.
 * Test FAIL in case ToC is found invalid.
 */
test_result_t test_fwu_toc(void)
{
	STATUS status;
	unsigned int toc_header;
	smc_args args = { SMC_PSCI_SYSTEM_RESET };
	smc_ret_values ret = {0};

	if (tftf_is_rebooted()) {
		/*
		 * Check whether we successfully resumed from the
		 * Firmware Update process. If we have, then the
		 * ToC header value will have been repaired.
		 */
		status = fwu_nvm_read(0, &toc_header, 4);
		if (status != STATUS_SUCCESS) {
			tftf_testcase_printf("Failed to read NVM (%d)\n", status);
			return TEST_RESULT_FAIL;
		}

		if (toc_header != TOC_HEADER_NAME) {
			tftf_testcase_printf("ToC is Invalid (%u)\n", toc_header);
			return TEST_RESULT_FAIL;
		}

		return TEST_RESULT_SUCCESS;
	}

	/* Corrupt the TOC in fip.bin. */
	toc_header = 0xdeadbeef;
	status = fwu_nvm_write(0, &toc_header, 4);
	if (status != STATUS_SUCCESS) {
		tftf_testcase_printf("Failed to overwrite the ToC header (%d)\n",
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
