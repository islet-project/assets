/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_nor_flash.h>
#include <io_storage.h>
#include <platform.h>
#include <tftf.h>
#include "platform_def.h"

#pragma weak plat_get_nvm_handle

/* IO devices */
static const io_dev_connector_t *flash_dev_con;
static uintptr_t flash_dev_spec;
static uintptr_t flash_init_params;
static uintptr_t flash_dev_handle;
static uintptr_t flash_handle;
static unsigned int flash_init;

static const io_nor_flash_spec_t flash_main_block_spec = {
	.device_address = FLASH_BASE,
	.region_address = FLASH_BASE,
	.block_size = NOR_FLASH_BLOCK_SIZE,
	.block_count = FLASH_SIZE / NOR_FLASH_BLOCK_SIZE
};

int arm_io_setup(void)
{
	int io_result;

	io_result = register_io_dev_nor_flash(&flash_dev_con);
	if (io_result != IO_SUCCESS)
		return io_result;

	io_result = io_dev_open(flash_dev_con, flash_dev_spec,
				&flash_dev_handle);
	if (io_result != IO_SUCCESS)
		return io_result;

	io_result = io_dev_init(flash_dev_handle, flash_init_params);
	if (io_result != IO_SUCCESS)
		return io_result;

	io_result = io_open(flash_dev_handle,
				(uintptr_t)&flash_main_block_spec,
				 &flash_handle);

	if (io_result == IO_SUCCESS)
		flash_init = 1;
	return io_result;
}

void plat_get_nvm_handle(uintptr_t *handle)
{
	assert(handle);
	assert(flash_init);

	*handle = flash_handle;
}

