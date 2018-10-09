/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __IMAGE_LOADER_H__
#define __IMAGE_LOADER_H__

#include <firmware_image_package.h>
#include <stddef.h>
#include <stdint.h>

/* Generic function to get flash offset of an image */
unsigned long get_image_offset(unsigned int image_id);

/* Generic function to return the size of an image */
unsigned long get_image_size(unsigned int image_id);

/*
 * Generic function to load an image at a specific address given an image id
 * Returns 0 on success, a negative error code otherwise.
 */
int load_image(unsigned int image_id,
		uintptr_t image_base);

/*
 * Generic function to load partial image at a specific address given
 * an image id. The flag argument is used to indicate last block to be
 * loaded in the partial loading scenario. If is_last_block == 0 then
 * devices are closed else they are kept open for partial image loading.
 * Returns 0 on success, a negative error code otherwise.
 */
int load_partial_image(unsigned int image_id,
		uintptr_t image_base,
		size_t image_size,
		unsigned int is_last_block);

/* This is to keep track of file related data. */
typedef struct {
	unsigned int file_pos;
	fip_toc_entry_t entry;
} fip_file_state_t;

#endif /* __IMAGE_LOADER_H__ */
