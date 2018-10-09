/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <io_storage.h>
#include <platform.h>
#include <platform_def.h>
#include <spinlock.h>
#include <status.h>
#include <string.h>
#include <tftf_lib.h>

#if USE_NVM
/* Used to serialize write operations from different CPU's */
static spinlock_t flash_access_lock;
#endif

STATUS tftf_nvm_write(unsigned long long offset, const void *buffer, size_t size)
{
#if USE_NVM
	int ret;
	uintptr_t nvm_handle;
	size_t length_written;
#endif

	if (offset + size > TFTF_NVM_SIZE)
		return STATUS_OUT_OF_RESOURCES;

#if USE_NVM
	/* Obtain a handle to the NVM by querying the platfom layer */
	plat_get_nvm_handle(&nvm_handle);

	spin_lock(&flash_access_lock);

	ret = io_seek(nvm_handle, IO_SEEK_SET,
					offset + TFTF_NVM_OFFSET);
	if (ret != IO_SUCCESS)
		goto fail;

	ret = io_write(nvm_handle, (const uintptr_t)buffer, size,
							 &length_written);
	if (ret != IO_SUCCESS)
		goto fail;

	assert(length_written == size);
fail:
	spin_unlock(&flash_access_lock);

	if (ret != IO_SUCCESS)
		return STATUS_FAIL;

#else
	uintptr_t addr = DRAM_BASE + TFTF_NVM_OFFSET + offset;
	memcpy((void *)addr, buffer, size);
#endif

	return STATUS_SUCCESS;
}

STATUS tftf_nvm_read(unsigned long long offset, void *buffer, size_t size)
{
#if USE_NVM
	int ret;
	uintptr_t nvm_handle;
	size_t length_read;
#endif

	if (offset + size > TFTF_NVM_SIZE)
		return STATUS_OUT_OF_RESOURCES;

#if USE_NVM
	/* Obtain a handle to the NVM by querying the platfom layer */
	plat_get_nvm_handle(&nvm_handle);

	spin_lock(&flash_access_lock);

	ret = io_seek(nvm_handle, IO_SEEK_SET, TFTF_NVM_OFFSET + offset);
	if (ret != IO_SUCCESS)
		goto fail;

	ret = io_read(nvm_handle, (uintptr_t)buffer, size, &length_read);
	if (ret != IO_SUCCESS)
		goto fail;

	assert(length_read == size);
fail:
	spin_unlock(&flash_access_lock);

	if (ret != IO_SUCCESS)
		return STATUS_FAIL;
#else
	uintptr_t addr = DRAM_BASE + TFTF_NVM_OFFSET + offset;
	memcpy(buffer, (void *)addr, size);
#endif

	return STATUS_SUCCESS;
}

