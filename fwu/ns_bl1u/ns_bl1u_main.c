/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <bl1.h>
#include <debug.h>
#include <drivers/io/io_fip.h>
#include <errno.h>
#include <image_loader.h>
#include <io_storage.h>
#include <mmio.h>
#include <nvm.h>
#include <platform.h>
#include <platform_def.h>
#include <smccc.h>
#include <status.h>
#include <string.h>
#include <tftf.h>
#include <tftf_lib.h>

#define FWU_NON_SECURE		(0x0)
#define FWU_SECURE		(0x1)

#define FWU_NON_EXEC		(0x0)
#define FWU_EXEC		(0x1)

/* This size is used to exercise partial copy */
#define FWU_COPY_PARTIAL_SIZE		(0x10)

extern const char version_string[];

typedef void (*ns_bl2u_entrypoint_t)(unsigned long);

void ns_bl1u_fwu_test_main(void);

/*
 * This structure will be used for:
 * 1. Assigning unique image identifier.
 * 2. Assigning attribute to FWU image.
 *    (FWU_NON_SECURE/FWU_SECURE)
 *    (FWU_NON_EXEC/FWU_EXEC)
 */
typedef struct fwu_image_load_desc {
	unsigned int image_id;
	unsigned int secure;
	unsigned int execute;
} fwu_image_load_desc_t;

static const fwu_image_load_desc_t ns_bl1u_desc[] = {
	[0] = {
		/* Initialize FWU_CERT image params */
		.image_id = FWU_CERT_ID,
		.secure = FWU_SECURE,
	},
	[1] = {
		/*
		 * Initialize SCP_BL2U image params
		 * Not needed for FVP platform.
		 */
		.image_id = SCP_BL2U_IMAGE_ID,
		.secure = FWU_SECURE,
	},
	[2] = {
		/* Initialize BL2U image params */
		.image_id = BL2U_IMAGE_ID,
		.secure = FWU_SECURE,
		.execute = FWU_EXEC
	},
	[3] = {
		/* Initialize NS_BL2U image params */
		.image_id = NS_BL2U_IMAGE_ID,
	}
};

static long smc_result;

#define CHECK_SMC_RESULT(_r)		do {	\
		if (smc_result != _r) {		\
			ERROR("NS_BL1U: SMC call failed with result:%li\n", smc_result);\
			panic();		\
		}		\
	} while (0);

static void ns_bl1u_fwu_smc_call(unsigned int smc_id,
			unsigned long x1,
			unsigned long x2,
			unsigned long x3,
			unsigned long x4)
{
	smc_ret_values fwu_result = {0};
	smc_args fwu_params = {smc_id, x1, x2, x3, x4};
	fwu_result = tftf_smc(&fwu_params);
	smc_result = fwu_result.ret0;
}


/*******************************************************************************
 * Following are the responsibilities of NS_BL1U image:
 * Load FWU images from external NVM memory to NS RAM.
 * Call SMC's to authenticate images.
 * Jump to NS_BL2U which carries out next FWU steps.
******************************************************************************/
void ns_bl1u_main(void)
{
	int index;
	unsigned int img_size;
	int err;
	unsigned long offset;
	ns_bl2u_entrypoint_t ns_bl2u_entrypoint =
			(ns_bl2u_entrypoint_t)NS_BL2U_BASE;
	const fwu_image_load_desc_t *image_desc;

	NOTICE("NS_BL1U: %s\n", version_string);
	NOTICE("NS_BL1U: %s\n", build_message);

	tftf_arch_setup();

	plat_fwu_io_setup();

#if FWU_BL_TEST
	ns_bl1u_fwu_test_main();
#endif

	for (index = 0; index < ARRAY_SIZE(ns_bl1u_desc); index++) {

		image_desc = &ns_bl1u_desc[index];

#if PLAT_fvp
		/* Skip SCP_BL2U loading for FVP */
		if (image_desc->image_id == SCP_BL2U_IMAGE_ID)
			continue;
#endif

		INFO("NS_BL1U: Loading image '%s' (ID:%u)\n",
			get_image_name(image_desc->image_id),
			image_desc->image_id);

		img_size = get_image_size(image_desc->image_id);
		INFO("NS_BL1U: Image size = %d\n", img_size);
		if (img_size == 0) {
			ERROR("NS_BL1U: Invalid image size\n");
			panic();
		}

		if (image_desc->secure == FWU_SECURE) {

			offset = get_image_offset(image_desc->image_id);

			INFO("NS_BL1U: Calling COPY SMC for partial copy\n");
			ns_bl1u_fwu_smc_call(FWU_SMC_IMAGE_COPY, image_desc->image_id,
				offset, FWU_COPY_PARTIAL_SIZE, img_size);
			CHECK_SMC_RESULT(0);

			ns_bl1u_fwu_smc_call(FWU_SMC_IMAGE_COPY, image_desc->image_id,
				(offset + FWU_COPY_PARTIAL_SIZE),
				(img_size - FWU_COPY_PARTIAL_SIZE), img_size);
			CHECK_SMC_RESULT(0);
		} else {
			/* The only non-secure image in ns_bl1u_desc[] should be NS_BL2U */
			assert(image_desc->image_id == NS_BL2U_IMAGE_ID);

			err = load_image(image_desc->image_id, NS_BL2U_BASE);
			if (err) {
				ERROR("NS_BL1U: Failed to load NS_BL2U\n");
				panic();
			}
			offset = NS_BL2U_BASE;
		}

		INFO("NS_BL1U: Calling AUTH SMC\n");
		ns_bl1u_fwu_smc_call(FWU_SMC_IMAGE_AUTH, image_desc->image_id,
				offset, img_size, 0);
		CHECK_SMC_RESULT(0);
#if FWU_BL_TEST
		/* Check if authenticating again the same image returns error. */
		INFO("NS_BL1U: TEST Calling SMC to auth again\n");
		ns_bl1u_fwu_smc_call(FWU_SMC_IMAGE_AUTH, ns_bl1u_desc[index].image_id,
				offset, img_size, 0);
		CHECK_SMC_RESULT(-EPERM);
#endif
		if (image_desc->execute == FWU_EXEC) {
			INFO("NS_BL1U: Calling EXECUTE SMC\n");
			ns_bl1u_fwu_smc_call(FWU_SMC_IMAGE_EXECUTE, image_desc->image_id,
					0, 0, 0);
			CHECK_SMC_RESULT(0);

#if FWU_BL_TEST
			/* Check if executing again the same image returns error. */
			INFO("NS_BL1U: TEST Calling SMC to execute again\n");
			ns_bl1u_fwu_smc_call(FWU_SMC_IMAGE_EXECUTE,
				ns_bl1u_desc[index].image_id, 0, 0, 0);
			CHECK_SMC_RESULT(-EPERM);
#endif
		} else {
			/*
			 * If the image is not executable, its internal state
			 * needs to be reset, unless it is for later consumption
			 * by another CPU (like for the SCP_BL2U firmware).
			 */
			if (image_desc->image_id != SCP_BL2U_IMAGE_ID) {
				INFO("NS_BL1U: Calling RESET SMC\n");
				ns_bl1u_fwu_smc_call(FWU_SMC_IMAGE_RESET,
						image_desc->image_id,
						0, 0, 0);
				CHECK_SMC_RESULT(0);
			}
		}
	}

	/*
	 * Clean and invalidate the caches.
	 * And disable the MMU before jumping to NS_BL2U.
	 */
	disable_mmu_icache();

	/*
	 * The argument passed to NS_BL2U is not used currently.
	 * But keeping the argument passing mechanism for future use.
	 */
	ns_bl2u_entrypoint(0);

	panic();
}
