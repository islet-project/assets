/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Test the FWU SMC interface in Trusted Firmware-A, which is implemented in
 * BL1.
 ******************************************************************************/

#include <bl1.h>
#include <debug.h>
#include <drivers/io/io_fip.h>
#include <errno.h>
#include <platform_def.h>
#include <smccc.h>
#include <status.h>
#include <tftf.h>
#include <tftf_lib.h>

typedef struct {
	/* Description to print before sending the SMC */
	const char *description;
	/* The arguments to pass to the SMC */
	const smc_args args;
	/* The expected SMC return value */
	u_register_t expect;
} ns_bl1u_test_t;

/*
 * The tests consist in sending a succession of SMCs to trigger FWU operations
 * in BL1. The order of the SMCs is important because they internally change the
 * FWU state machine in Trusted Firmware-A.
 */
static const ns_bl1u_test_t tests[] = {
	/* Basic FWU SMC handler test cases. */
	{
		.description	= "BL1_SMC_CALL_COUNT",
		.args		= { BL1_SMC_CALL_COUNT, 0, 0, 0, 0 },
		.expect		= BL1_NUM_SMC_CALLS,
	},

	{
		.description	= "BL1_SMC_VERSION",
		.args		= { BL1_SMC_VERSION, 0, 0, 0, 0 },
		.expect		= BL1_VERSION,
	},

	{
		.description	= "Invalid SMC",
		.args		= { 0xdeadbeef, 0, 0, 0, 0 },
		.expect		= SMC_UNKNOWN,
	},

	/* FWU_SMC_IMAGE_COPY test cases. */
	{
		.description	= "IMAGE_COPY with invalid image_id",
		.args		= { FWU_SMC_IMAGE_COPY, 0xdeadbeef, 0, 0, 0 },
		.expect		= -EPERM,
	},

	{
		.description	= "IMAGE_COPY with non-secure image_id",
		.args		= { FWU_SMC_IMAGE_COPY, NS_BL2U_IMAGE_ID, 0, 0, 0 },
		.expect		= -EPERM,
	},

	{
		.description	= "IMAGE_COPY with valid args",
		.args		= { FWU_SMC_IMAGE_COPY, FWU_CERT_ID, PLAT_ARM_FWU_FIP_BASE, 0x20, 0x20 },
		.expect		= STATUS_SUCCESS,
	},

	{
		.description	= "IMAGE_COPY to copy an image_id again",
		.args		= { FWU_SMC_IMAGE_COPY, FWU_CERT_ID, PLAT_ARM_FWU_FIP_BASE, 0x20, 0x20 },
		.expect		= -EPERM,
	},

	{
		.description	= "IMAGE_COPY with source address not mapped",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, 0, 0, 0 },
		.expect		= -ENOMEM,
	},

	{
		.description	= "IMAGE_COPY with source size not mapped",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0xdeadbeef, 0xdeadbeef },
		.expect		= -ENOMEM,
	},

	{
		.description	= "IMAGE_COPY with image size more than secure mem",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0x40000, 0x40000 },
		.expect		= -ENOMEM,
	},

	{
		.description	= "IMAGE_COPY with image size 0",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0, 0 },
		.expect		= -ENOMEM,
	},

	/*
	 * At this point, the FWU Certificate Image has been copied by a
	 * previous test. Try to load the BL2U image over it at the same
	 * address.
	 */
	{
		.description	= "IMAGE_COPY with an image that overlaps a different one",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0x20, 0x40 },
		.expect		= -EPERM,
	},

	/*
	 * The authentication of the FWU certificate will fail, which will set
	 * the state of this image to "RESET" for the following tests.
	 */
	{
		.description	= "IMAGE_AUTH with an invalid image",
		.args		= { FWU_SMC_IMAGE_AUTH, FWU_CERT_ID, PLAT_ARM_FWU_FIP_BASE, 0x20, 0x20 },
		.expect		= -EAUTH,
	},

	{
		.description	= "IMAGE_COPY with 1st block size in partial copy",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0x20, 0x40 },
		.expect		= STATUS_SUCCESS,
	},

	{
		.description	= "IMAGE_AUTH while copying the image",
		.args		= { FWU_SMC_IMAGE_AUTH, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0x40, 0 },
		.expect		= -EPERM,
	},

	{
		.description	= "IMAGE_COPY with last block with invalid source in partial copy",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, 0, 0x21, 0x40 },
		.expect		= -ENOMEM,
	},

	{
		.description	= "IMAGE_COPY with last block size > total size in partial copy",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0x21, 0x40 },
		.expect		= STATUS_SUCCESS,
	},

	{
		.description	= "IMAGE_AUTH to RESET the image state",
		.args		= { FWU_SMC_IMAGE_AUTH, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0x40, 0 },
		.expect		= -EAUTH,
	},

	{
		.description	= "IMAGE_COPY with block size > total size",
		.args		= { FWU_SMC_IMAGE_COPY, BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0x21, 0x20 },
		.expect		= STATUS_SUCCESS,
	},

	{
		.description	= "IMAGE_RESET to RESET the image state",
		.args		= { FWU_SMC_IMAGE_RESET, BL2U_IMAGE_ID, 0, 0, 0 },
		.expect		= STATUS_SUCCESS,
	},


	/* FWU_SMC_IMAGE_AUTH test cases. */
	{
		.description	= "IMAGE_AUTH with invalid image_id",
		.args		= { FWU_SMC_IMAGE_AUTH, 0, 0, 0, 0 },
		.expect		= -EPERM,
	},

	{
		.description	= "IMAGE_AUTH with secure image not copied",
		.args		= { FWU_SMC_IMAGE_AUTH, BL2U_IMAGE_ID, 0, 0, 0 },
		.expect		= -EPERM,
	},

	{
		.description	= "IMAGE_AUTH with source address not mapped",
		.args		= { FWU_SMC_IMAGE_AUTH, NS_BL2U_IMAGE_ID, 0, 0, 0 },
		.expect		= -ENOMEM,
	},

	{
		.description	= "IMAGE_AUTH with source size not mapped",
		.args		= { FWU_SMC_IMAGE_AUTH, NS_BL2U_IMAGE_ID, PLAT_ARM_FWU_FIP_BASE, 0xdeadbeef, 0 },
		.expect		= -ENOMEM,
	},

	{
		.description	= "IMAGE_COPY to copy after auth failure",
		.args		= { FWU_SMC_IMAGE_COPY, FWU_CERT_ID, PLAT_ARM_FWU_FIP_BASE, 0x40, 0x40 },
		.expect		= STATUS_SUCCESS,
	},

	{
		.description	= "IMAGE_AUTH with valid args for copied image",
		.args		= { FWU_SMC_IMAGE_AUTH, FWU_CERT_ID, 0, 0, 0 },
		.expect		= -EAUTH,
	},

	/* FWU_SMC_IMAGE_EXECUTE test cases. */
	{
		.description	= "IMAGE_EXECUTE with invalid image_id",
		.args		= { FWU_SMC_IMAGE_EXECUTE, 0, 0, 0, 0 },
		.expect		= -EPERM,
	},

	{
		.description	= "IMAGE_EXECUTE with non-executable image_id",
		.args		= { FWU_SMC_IMAGE_EXECUTE, FWU_CERT_ID, 0, 0, 0 },
		.expect		= -EPERM,
	},

	{
		.description	= "IMAGE_EXECUTE with un-authenticated image_id",
		.args		= { FWU_SMC_IMAGE_EXECUTE, BL2U_IMAGE_ID, 0, 0, 0 },
		.expect		= -EPERM,
	},


	/* FWU_SMC_IMAGE_RESUME test case. */
	{
		.description	= "IMAGE_RESUME with invalid args",
		.args		= { FWU_SMC_IMAGE_RESUME, 0, 0, 0, 0 },
		.expect		= -EPERM,
	},
};


void ns_bl1u_fwu_test_main(void)
{
	NOTICE("NS_BL1U: ***** Starting NS_BL1U FWU test *****\n");

	for (int i = 0 ; i < ARRAY_SIZE(tests); ++i) {
		u_register_t result;

		INFO("NS_BL1U: %s\n", tests[i].description);

		smc_ret_values smc_ret;
		smc_ret = tftf_smc(&tests[i].args);
		result = smc_ret.ret0;

		if (result != tests[i].expect) {
			ERROR("NS_BL1U: Unexpected SMC return value 0x%lX, "
				"expected 0x%lX\n", result, tests[i].expect);
			panic();
		}
	}

	NOTICE("NS_BL1U: ***** All FWU test passed *****\n\n");
}
