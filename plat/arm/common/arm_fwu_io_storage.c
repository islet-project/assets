/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_fip.h>
#include <drivers/io/io_memmap.h>
#include <firmware_image_package.h>
#include <image_loader.h>
#include <io_storage.h>
#include <platform.h>
#include <platform_def.h>
#include <string.h>
#include <tftf_lib.h>

/* May be overridden in specific ARM standard platform */
#pragma weak plat_arm_fwu_io_setup

/* IO devices */
static const io_dev_connector_t *fwu_fip_dev_con;
static uintptr_t fwu_fip_dev_handle;
static const io_dev_connector_t *memmap_dev_con;
static uintptr_t memmap_dev_handle;

static const io_block_spec_t fwu_fip_block_spec = {
	.offset = PLAT_ARM_FWU_FIP_BASE,
	.length = PLAT_ARM_FWU_FIP_SIZE
};
static const io_uuid_spec_t fwu_cert_uuid_spec = {
	.uuid = UUID_FIRMWARE_UPDATE_FWU_CERT,
};
static const io_uuid_spec_t scp_bl2u_uuid_spec = {
	.uuid = UUID_FIRMWARE_UPDATE_SCP_BL2U,
};
static const io_uuid_spec_t bl2u_uuid_spec = {
	.uuid = UUID_FIRMWARE_UPDATE_BL2U,
};
static const io_uuid_spec_t ns_bl2u_uuid_spec = {
	.uuid = UUID_FIRMWARE_UPDATE_NS_BL2U,
};

static int open_fwu_fip(const uintptr_t spec);
static int open_memmap(const uintptr_t spec);

struct plat_io_policy {
	uintptr_t *dev_handle;
	uintptr_t image_spec;
	int (*check)(const uintptr_t spec);
};

static const struct plat_io_policy policies[] = {
	[FWU_FIP_IMAGE_ID] = {
		&memmap_dev_handle,
		(uintptr_t)&fwu_fip_block_spec,
		open_memmap
	},
	[FWU_CERT_ID] = {
		&fwu_fip_dev_handle,
		(uintptr_t)&fwu_cert_uuid_spec,
		open_fwu_fip
	},
	[SCP_BL2U_IMAGE_ID] = {
		&fwu_fip_dev_handle,
		(uintptr_t)&scp_bl2u_uuid_spec,
		open_fwu_fip
	},
	[BL2U_IMAGE_ID] = {
		&fwu_fip_dev_handle,
		(uintptr_t)&bl2u_uuid_spec,
		open_fwu_fip
	},
	[NS_BL2U_IMAGE_ID] = {
		&fwu_fip_dev_handle,
		(uintptr_t)&ns_bl2u_uuid_spec,
		open_fwu_fip
	},
};


/* Weak definitions may be overridden in each specific platform */
#pragma weak plat_get_image_source

static int open_fwu_fip(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	/* See if a Firmware Image Package is available */
	result = io_dev_init(fwu_fip_dev_handle,
						(uintptr_t)FWU_FIP_IMAGE_ID);
	if (result == IO_SUCCESS) {
		result = io_open(fwu_fip_dev_handle, spec,
						&local_image_handle);
		if (result == IO_SUCCESS) {
			VERBOSE("Using FIP\n");
			io_close(local_image_handle);
		}
	}
	return result;
}


static int open_memmap(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	result = io_dev_init(memmap_dev_handle, (uintptr_t)NULL);
	if (result == IO_SUCCESS) {
		result = io_open(memmap_dev_handle, spec, &local_image_handle);
		if (result == IO_SUCCESS) {
			VERBOSE("Using Memmap\n");
			io_close(local_image_handle);
		}
	}
	return result;
}


void plat_arm_fwu_io_setup(void)
{
	int io_result;

	io_result = register_io_dev_fip(&fwu_fip_dev_con);
	assert(io_result == IO_SUCCESS);

	io_result = register_io_dev_memmap(&memmap_dev_con);
	assert(io_result == IO_SUCCESS);

	/* Open connections to devices and cache the handles */
	io_result = io_dev_open(fwu_fip_dev_con, (uintptr_t)NULL,
				&fwu_fip_dev_handle);
	assert(io_result == IO_SUCCESS);

	io_result = io_dev_open(memmap_dev_con, (uintptr_t)NULL,
				&memmap_dev_handle);
	assert(io_result == IO_SUCCESS);

	/* Ignore improbable errors in release builds */
	(void)io_result;
}


/* Return an IO device handle and specification which can be used to access
 * an image. Use this to enforce platform load policy */
int plat_get_image_source(unsigned int image_id, uintptr_t *dev_handle,
			  uintptr_t *image_spec)
{
	int result = IO_FAIL;
	const struct plat_io_policy *policy;

	assert(image_id < ARRAY_SIZE(policies));

	policy = &policies[image_id];
	result = policy->check(policy->image_spec);
	if (result == IO_SUCCESS) {
		*image_spec = policy->image_spec;
		*dev_handle = *(policy->dev_handle);
	}

	return result;
}

void plat_fwu_io_setup(void)
{
	plat_arm_fwu_io_setup();
}
