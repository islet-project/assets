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

unsigned long get_image_offset(unsigned int image_id)
{
	uintptr_t dev_handle;
	uintptr_t image_handle;
	uintptr_t image_spec;
	unsigned long img_offset;
	int io_result;
	io_entity_t *entity;
	fip_file_state_t *fp;

	/* Obtain a reference to the image by querying the platform layer */
	io_result = plat_get_image_source(image_id, &dev_handle, &image_spec);
	if (io_result != IO_SUCCESS) {
		WARN("Failed to obtain reference to image id=%u (%i)\n",
			image_id, io_result);
		return 0;
	}

	/* Attempt to access the image */
	io_result = io_open(dev_handle, image_spec, &image_handle);
	if (io_result != IO_SUCCESS) {
		WARN("Failed to access image id=%u (%i)\n",
			image_id, io_result);
		return 0;
	}

	entity = (io_entity_t *)image_handle;

	fp = (fip_file_state_t *)entity->info;
	img_offset = PLAT_ARM_FWU_FIP_BASE + fp->entry.offset_address;

	(void)io_close(image_handle);
	(void)io_dev_close(dev_handle);

	return img_offset;
}


unsigned long get_image_size(unsigned int image_id)
{
	uintptr_t dev_handle;
	uintptr_t image_handle;
	uintptr_t image_spec;
	size_t image_size;
	int io_result;

	/* Obtain a reference to the image by querying the platform layer */
	io_result = plat_get_image_source(image_id, &dev_handle, &image_spec);
	if (io_result != IO_SUCCESS) {
		WARN("Failed to obtain reference to image id=%u (%i)\n",
			image_id, io_result);
		return 0;
	}

	/* Attempt to access the image */
	io_result = io_open(dev_handle, image_spec, &image_handle);
	if (io_result != IO_SUCCESS) {
		WARN("Failed to access image id=%u (%i)\n",
			image_id, io_result);
		return 0;
	}

	/* Find the size of the image */
	io_result = io_size(image_handle, &image_size);
	if ((io_result != IO_SUCCESS) || (image_size == 0)) {
		WARN("Failed to determine the size of the image id=%u (%i)\n",
			image_id, io_result);
	}
	io_result = io_close(image_handle);
	io_result = io_dev_close(dev_handle);

	return image_size;
}


int load_image(unsigned int image_id, uintptr_t image_base)
{
	uintptr_t dev_handle;
	uintptr_t image_handle;
	uintptr_t image_spec;
	size_t image_size;
	size_t bytes_read;
	int io_result;

	/* Obtain a reference to the image by querying the platform layer */
	io_result = plat_get_image_source(image_id, &dev_handle, &image_spec);
	if (io_result != IO_SUCCESS) {
		WARN("Failed to obtain reference to image id=%u (%i)\n",
			image_id, io_result);
		return io_result;
	}

	/* Attempt to access the image */
	io_result = io_open(dev_handle, image_spec, &image_handle);
	if (io_result != IO_SUCCESS) {
		WARN("Failed to access image id=%u (%i)\n",
			image_id, io_result);
		return io_result;
	}

	INFO("Loading image id=%u at address %p\n", image_id, (void *)image_base);

	/* Find the size of the image */
	io_result = io_size(image_handle, &image_size);
	if ((io_result != IO_SUCCESS) || (image_size == 0)) {
		WARN("Failed to determine the size of the image id=%u (%i)\n",
			image_id, io_result);
		goto exit;
	}

	/* Load the image now */
	io_result = io_read(image_handle, image_base, image_size, &bytes_read);
	if ((io_result != IO_SUCCESS) || (bytes_read < image_size)) {
		WARN("Failed to load image id=%u (%i)\n", image_id, io_result);
		goto exit;
	}

	/*
	 * File has been successfully loaded.
	 * Flush the image so that the next EL can see it.
	 */
	flush_dcache_range(image_base, image_size);

	INFO("Image id=%u loaded: %p - %p\n", image_id, (void *)image_base,
	     (void *)(image_base + image_size - 1));

exit:
	io_close(image_handle);
	io_dev_close(dev_handle);
	return io_result;
}


int load_partial_image(unsigned int image_id,
		uintptr_t image_base,
		size_t image_size,
		unsigned int is_last_block)
{
	static uintptr_t dev_handle;
	static uintptr_t image_handle;
	uintptr_t image_spec;
	size_t bytes_read;
	int io_result;

	if (!image_handle) {
		/* Obtain a reference to the image by querying the platform layer */
		io_result = plat_get_image_source(image_id, &dev_handle, &image_spec);
		if (io_result != IO_SUCCESS) {
			WARN("Failed to obtain reference to image id=%u (%i)\n",
				image_id, io_result);
			return io_result;
		}

		/* Attempt to access the image */
		io_result = io_open(dev_handle, image_spec, &image_handle);
		if (io_result != IO_SUCCESS) {
			WARN("Failed to access image id=%u (%i)\n",
				image_id, io_result);
			return io_result;
		}
	}

	INFO("Loading image id=%u at address %p\n", image_id, (void *)image_base);

	io_result = io_read(image_handle, image_base, image_size, &bytes_read);
	if ((io_result != IO_SUCCESS) || (bytes_read < image_size)) {
		WARN("Failed to load image id=%u (%i)\n", image_id, io_result);
		is_last_block = 0;
		goto exit;
	}

	/*
	 * File has been successfully loaded.
	 * Flush the image so that the next EL can see it.
	 */
	flush_dcache_range(image_base, image_size);

	INFO("Image id=%u loaded: %p - %p\n", image_id, (void *)image_base,
	     (void *)(image_base + image_size - 1));

exit:

	if (is_last_block == 0) {
		io_close(image_handle);
		io_dev_close(dev_handle);
		image_handle = 0;
	}
	return io_result;
}

