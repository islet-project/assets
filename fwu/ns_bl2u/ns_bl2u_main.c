/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <bl1.h>
#include <debug.h>
#include <firmware_image_package.h>
#include <fwu_nvm.h>
#include <platform.h>
#include <platform_def.h>

extern const char version_string[];

void ns_bl2u_main(void)
{
	smc_args fwu_params = {0};
	smc_ret_values fwu_result = {0};

	NOTICE("NS_BL2U: %s\n", version_string);
	NOTICE("NS_BL2U: %s\n", build_message);

	tftf_platform_setup();

#if FWU_BL_TEST
	unsigned int toc_header;
	STATUS status = fwu_nvm_read(0, &toc_header, 4);
	if (status != STATUS_SUCCESS) {
		ERROR("NS_BL2U: Failed to read NVM\n");
		panic();
	}

	/* Update the TOC if found invalid. */
	if (toc_header != TOC_HEADER_NAME) {
		toc_header = TOC_HEADER_NAME;
		status = fwu_nvm_write(0, &toc_header, 4);
		if (status != STATUS_SUCCESS) {
			ERROR("NS_BL2U: Failed to update TOC value\n");
			panic();
		}
		INFO("NS_BL2U: Updated TOC value\n");
	}
#endif

	/* Update the FIP image. */
	if (fwu_update_fip(FIP_BKP_ADDRESS) != STATUS_SUCCESS) {
		ERROR("NS_BL2U: Firmware Image Update Failed\n");
		panic();
	}

	/* Call FWU_SMC_UPDATE_DONE to indicate image update done. */
	INFO("NS_BL2U: Calling FWU_SMC_UPDATE_DONE\n");
	fwu_params.fid = FWU_SMC_UPDATE_DONE;
	fwu_result = tftf_smc(&fwu_params);
	ERROR("NS_BL2U: Unexpected return from FWU process (%d)\n",
			(int)fwu_result.ret0);
	panic();
}
