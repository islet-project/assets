/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <drivers/io/io_driver.h>
#include <io_storage.h>
#include <string.h>

/* As we need to be able to keep state for seek, only one file can be open
 * at a time. Make this a structure and point to the entity->info. When we
 * can malloc memory we can change this to support more open files.
 */
typedef struct {
	/* Use the 'in_use' flag as any value for base and file_pos could be
	 * valid.
	 */
	int		in_use;
	uintptr_t	base;
	size_t		file_pos;
} file_state_t;

static file_state_t current_file = {0};

/* Identify the device type as memmap */
io_type_t device_type_memmap(void)
{
	return IO_TYPE_MEMMAP;
}

/* Memmap device functions */
static int memmap_dev_open(const uintptr_t dev_spec, io_dev_info_t **dev_info);
static int memmap_block_open(io_dev_info_t *dev_info, const uintptr_t spec,
			     io_entity_t *entity);
static int memmap_block_seek(io_entity_t *entity, int mode,
			     ssize_t offset);
static int memmap_block_read(io_entity_t *entity, uintptr_t buffer,
			     size_t length, size_t *length_read);
static int memmap_block_write(io_entity_t *entity, const uintptr_t buffer,
			      size_t length, size_t *length_written);
static int memmap_block_close(io_entity_t *entity);
static int memmap_dev_close(io_dev_info_t *dev_info);


static const io_dev_connector_t memmap_dev_connector = {
	.dev_open = memmap_dev_open
};


static const io_dev_funcs_t memmap_dev_funcs = {
	.type = device_type_memmap,
	.open = memmap_block_open,
	.seek = memmap_block_seek,
	.size = NULL,
	.read = memmap_block_read,
	.write = memmap_block_write,
	.close = memmap_block_close,
	.dev_init = NULL,
	.dev_close = memmap_dev_close,
};


/* No state associated with this device so structure can be const */
static const io_dev_info_t memmap_dev_info = {
	.funcs = &memmap_dev_funcs,
	.info = (uintptr_t)NULL
};


/* Open a connection to the memmap device */
static int memmap_dev_open(const uintptr_t dev_spec __attribute__((unused)),
			   io_dev_info_t **dev_info)
{
	assert(dev_info != NULL);
	*dev_info = (io_dev_info_t *)&memmap_dev_info; /* cast away const */

	return IO_SUCCESS;
}



/* Close a connection to the memmap device */
static int memmap_dev_close(io_dev_info_t *dev_info)
{
	/* NOP */
	/* TODO: Consider tracking open files and cleaning them up here */
	return IO_SUCCESS;
}


/* Open a file on the memmap device */
/* TODO: Can we do any sensible limit checks on requested memory */
static int memmap_block_open(io_dev_info_t *dev_info, const uintptr_t spec,
			     io_entity_t *entity)
{
	int result = IO_FAIL;
	const io_block_spec_t *block_spec = (io_block_spec_t *)spec;

	/* Since we need to track open state for seek() we only allow one open
	 * spec at a time. When we have dynamic memory we can malloc and set
	 * entity->info.
	 */
	if (current_file.in_use == 0) {
		assert(block_spec != NULL);
		assert(entity != NULL);

		current_file.in_use = 1;
		current_file.base = block_spec->offset;
		/* File cursor offset for seek and incremental reads etc. */
		current_file.file_pos = 0;
		entity->info = (uintptr_t)&current_file;
		result = IO_SUCCESS;
	} else {
		WARN("A Memmap device is already active. Close first.\n");
		result = IO_RESOURCES_EXHAUSTED;
	}

	return result;
}


/* Seek to a particular file offset on the memmap device */
static int memmap_block_seek(io_entity_t *entity, int mode, ssize_t offset)
{
	int result = IO_FAIL;

	/* We only support IO_SEEK_SET for the moment. */
	if (mode == IO_SEEK_SET) {
		assert(entity != NULL);

		/* TODO: can we do some basic limit checks on seek? */
		((file_state_t *)entity->info)->file_pos = offset;
		result = IO_SUCCESS;
	} else {
		result = IO_FAIL;
	}

	return result;
}


/* Read data from a file on the memmap device */
static int memmap_block_read(io_entity_t *entity, uintptr_t buffer,
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


/* Write data to a file on the memmap device */
static int memmap_block_write(io_entity_t *entity, const uintptr_t buffer,
			      size_t length, size_t *length_written)
{
	file_state_t *fp;

	assert(entity != NULL);
	assert(buffer != (uintptr_t)NULL);
	assert(length_written != NULL);

	fp = (file_state_t *)entity->info;

	memcpy((void *)(fp->base + fp->file_pos), (void *)buffer, length);

	*length_written = length;

	/* advance the file 'cursor' for incremental writes */
	fp->file_pos += length;

	return IO_SUCCESS;
}


/* Close a file on the memmap device */
static int memmap_block_close(io_entity_t *entity)
{
	assert(entity != NULL);

	entity->info = 0;

	/* This would be a mem free() if we had malloc.*/
	memset((void *)&current_file, 0, sizeof(current_file));

	return IO_SUCCESS;
}


/* Exported functions */

/* Register the memmap driver with the IO abstraction */
int register_io_dev_memmap(const io_dev_connector_t **dev_con)
{
	int result = IO_FAIL;
	assert(dev_con != NULL);

	result = io_register_device(&memmap_dev_info);
	if (result == IO_SUCCESS)
		*dev_con = &memmap_dev_connector;

	return result;
}
