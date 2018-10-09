/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __IO_NOR_FLASH_H__
#define __IO_NOR_FLASH_H__

#include <platform_def.h>

#ifndef NOR_FLASH_BLOCK_SIZE
	#error NOR_FLASH_BLOCK_SIZE must be defined as the block \
		 size of the NOR Flash seen by the software
#endif

/* IO NOR Flash specification - used to refer to data on a memory map device
 * supporting block-like entities */
typedef struct io_nor_spec {
	/* Base Address of the NOR Flash device - it is required to program
	 * the flash */
	uintptr_t device_address;
	uintptr_t region_address;
	uint32_t block_size;
	uint32_t block_count;
} io_nor_flash_spec_t;

struct io_dev_connector;

int register_io_dev_nor_flash(const struct io_dev_connector **dev_con);

#endif /* __IO_NOR_FLASH_H__ */
