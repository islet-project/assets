/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <debug.h>
#include <string.h>
#include "io_vexpress_nor_internal.h"

static file_state_t current_file = {0};

/* Identify the device type as flash */
io_type_t device_type_flash(void)
{
	return IO_TYPE_FLASH;
}

/* NOR Flash device functions */
static int flash_dev_open(const uintptr_t dev_spec, io_dev_info_t **dev_info);
static int flash_open(io_dev_info_t *dev_info, const uintptr_t spec,
			     io_entity_t *entity);
static int flash_seek(io_entity_t *entity, int mode,
			     ssize_t offset);
static int flash_read(io_entity_t *entity, uintptr_t buffer,
			     size_t length, size_t *length_read);
static int flash_write(io_entity_t *entity, const uintptr_t buffer,
			      size_t length, size_t *length_written);
static int flash_close(io_entity_t *entity);
static int flash_dev_close(io_dev_info_t *dev_info);


static const io_dev_connector_t flash_dev_connector = {
	.dev_open = flash_dev_open
};

static const io_dev_funcs_t flash_dev_funcs = {
	.type = device_type_flash,
	.open = flash_open,
	.seek = flash_seek,
	.size = NULL,
	.read = flash_read,
	.write = flash_write,
	.close = flash_close,
	.dev_init = NULL,
	.dev_close = flash_dev_close,
};

/* No state associated with this device so structure can be const */
static const io_dev_info_t flash_dev_info = {
	.funcs = &flash_dev_funcs,
	.info = (uintptr_t)NULL
};

/* Open a connection to the flash device */
static int flash_dev_open(const uintptr_t dev_spec __attribute__((unused)),
			   io_dev_info_t **dev_info)
{
	assert(dev_info != NULL);
	*dev_info = (io_dev_info_t *)&flash_dev_info; /* cast away const */

	return IO_SUCCESS;
}

/* Close a connection to the flash device */
static int flash_dev_close(io_dev_info_t *dev_info)
{
	/* NOP */
	/* TODO: Consider tracking open files and cleaning them up here */
	return IO_SUCCESS;
}


/* Open a file on the flash device */
/* TODO: Can we do any sensible limit checks on requested memory */
static int flash_open(io_dev_info_t *dev_info, const uintptr_t spec,
			     io_entity_t *entity)
{
	int result;
	const io_nor_flash_spec_t *block_spec = (io_nor_flash_spec_t *)spec;

	/* Since we need to track open state for seek() we only allow one open
	 * spec at a time. When we have dynamic memory we can malloc and set
	 * entity->info.
	 */
	if (current_file.in_use == 0) {
		assert(block_spec != NULL);
		assert(entity != NULL);

		current_file.in_use = 1;
		current_file.base = block_spec->region_address;
		/* File cursor offset for seek and incremental reads etc. */
		current_file.file_pos = 0;
		/* Attach the device specification to this file */
		current_file.block_spec = block_spec;

		entity->info = (uintptr_t)&current_file;
		result = IO_SUCCESS;
	} else {
		WARN("A Flash device is already active. Close first.\n");
		result = IO_RESOURCES_EXHAUSTED;
	}

	return result;
}

/* Seek to a particular file offset on the flash device */
static int flash_seek(io_entity_t *entity, int mode, ssize_t offset)
{
	const io_nor_flash_spec_t *block_spec;
	int result;
	uintptr_t flash_base;
	size_t flash_size;

	assert(entity != NULL);

	block_spec = ((file_state_t *)entity->info)->block_spec;
	flash_size = block_spec->block_count * block_spec->block_size;

	if (mode == IO_SEEK_SET) {
		/* Ensure the offset is into the flash */
		if ((offset >= 0) && (offset < flash_size)) {
			((file_state_t *)entity->info)->file_pos = offset;
			result = IO_SUCCESS;
		} else {
			result = IO_FAIL;
		}
	} else if (mode == IO_SEEK_END) {
		flash_base = block_spec->region_address;
		/* Ensure the offset is into the flash */
		if ((offset <= 0) && (flash_size + offset >= 0))  {
			((file_state_t *)entity->info)->file_pos =
					flash_base + flash_size + offset;
			result = IO_SUCCESS;
		} else {
			result = IO_FAIL;
		}
	} else if (mode == IO_SEEK_CUR) {
		flash_base = block_spec->region_address;
		/* Ensure the offset is into the flash */
		if ((((file_state_t *)entity->info)->file_pos +
					offset >= flash_base) &&
		    (((file_state_t *)entity->info)->file_pos +
					offset < flash_base + flash_size)) {
			((file_state_t *)entity->info)->file_pos += offset;
			result = IO_SUCCESS;
		} else {
			result = IO_FAIL;
		}
	} else {
		result = IO_FAIL;
	}

	return result;
}

/* Read data from a file on the flash device */
static int flash_read(io_entity_t *entity, uintptr_t buffer,
			     size_t length, size_t *length_read)
{
	file_state_t *fp;

	assert(entity != NULL);
	assert(buffer != (uintptr_t)NULL);
	assert(length_read != NULL);

	fp = (file_state_t *)entity->info;

	memcpy((void *)buffer, (void *)(fp->base + fp->file_pos), length);

	*length_read = length;
	/* advance the file 'cursor' for incremental reads */
	fp->file_pos += length;

	return IO_SUCCESS;
}

/* Write data to a file on the flash device */
static int flash_write(io_entity_t *entity, const uintptr_t buffer,
			      size_t length, size_t *length_written)
{
	file_state_t *fp;
	const io_nor_flash_spec_t *block_spec;
	size_t file_pos;
	int ret;
	uint32_t remaining_block_size;
	size_t written;
	uintptr_t buffer_ptr = buffer;

	assert(entity != NULL);
	assert(buffer != (uintptr_t)NULL);
	assert(length_written != NULL);

	fp = (file_state_t *)entity->info;
	block_spec = fp->block_spec;
	file_pos = fp->file_pos;

	*length_written = 0;

	while (length > 0) {
		/* Check if we can do a block write */
		if (IS_FLASH_ADDRESS_BLOCK_ALIGNED(fp, file_pos)) {
			if (length / block_spec->block_size > 0) {
				ret = flash_block_write(fp, file_pos,
							 buffer_ptr, &written);
			} else {
				/* Case when the length is smaller than a
				 * block size
				 */
				ret = flash_partial_write(fp, file_pos,
					buffer_ptr, length, &written);
			}
		} else {
			/* Case when the buffer does not start at the beginning
			 * of a block
			 */

			/* Length between the current file_pos and the end of
			 * the block
			 */
			remaining_block_size = block_spec->block_size -
					(file_pos % block_spec->block_size);

			if (length < remaining_block_size) {
				ret = flash_partial_write(fp, file_pos,
						 buffer_ptr, length, &written);
			} else {
				ret = flash_partial_write(fp, file_pos,
					buffer_ptr, remaining_block_size,
					 &written);
			}
		}

		/* If one of the write operations fails then we do not pursue */
		if (ret != IO_SUCCESS) {
			return ret;
		} else {
			buffer_ptr += written;
			file_pos += written;

			*length_written += written;
			length -= written;
		}
	}

	/* advance the file 'cursor' for incremental writes */
	fp->file_pos += length;

	return IO_SUCCESS;
}

/* Close a file on the flash device */
static int flash_close(io_entity_t *entity)
{
	assert(entity != NULL);

	entity->info = 0;

	/* This would be a mem free() if we had malloc.*/
	memset((void *)&current_file, 0, sizeof(current_file));

	return IO_SUCCESS;
}

/* Exported functions */

/* Register the flash driver with the IO abstraction */
int register_io_dev_nor_flash(const io_dev_connector_t **dev_con)
{
	int result;
	assert(dev_con != NULL);

	result = io_register_device(&flash_dev_info);
	if (result == IO_SUCCESS)
		*dev_con = &flash_dev_connector;

	return result;
}
