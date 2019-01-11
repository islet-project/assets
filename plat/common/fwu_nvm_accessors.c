/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <drivers/io/io_fip.h>
#include <firmware_image_package.h>
#include <fwu_nvm.h>
#include <io_storage.h>
#include <platform.h>
#include <platform_def.h>
#include <status.h>
#include <string.h>
#include <uuid_utils.h>


STATUS fwu_nvm_write(unsigned long long offset, const void *buffer, size_t size)
{
	uintptr_t nvm_handle;
	int ret;
	size_t length_write;

	if (offset + size > FLASH_SIZE)
		return STATUS_OUT_OF_RESOURCES;

	/* Obtain a handle to the NVM by querying the platfom layer */
	plat_get_nvm_handle(&nvm_handle);

	/* Seek to the given offset. */
	ret = io_seek(nvm_handle, IO_SEEK_SET, offset);
	if (ret != IO_SUCCESS)
		return STATUS_FAIL;

	/* Write to the given offset. */
	ret = io_write(nvm_handle, (const uintptr_t)buffer,
		size, &length_write);
	if ((ret != IO_SUCCESS) || (size != length_write))
		return STATUS_FAIL;

	return STATUS_SUCCESS;
}

STATUS fwu_nvm_read(unsigned long long offset, void *buffer, size_t size)
{
	uintptr_t nvm_handle;
	int ret;
	size_t length_read;

	if (offset + size > FLASH_SIZE)
		return STATUS_OUT_OF_RESOURCES;

	/* Obtain a handle to the NVM by querying the platform layer */
	plat_get_nvm_handle(&nvm_handle);

	/* Seek to the given offset. */
	ret = io_seek(nvm_handle, IO_SEEK_SET, offset);
	if (ret != IO_SUCCESS)
		return STATUS_FAIL;

	/* Read from the given offset. */
	ret = io_read(nvm_handle, (const uintptr_t)buffer,
		size, &length_read);
	if ((ret != IO_SUCCESS) || (size != length_read))
		return STATUS_FAIL;

	return STATUS_SUCCESS;
}


STATUS fwu_update_fip(unsigned long fip_addr)
{
	uintptr_t nvm_handle;
	int ret;
	size_t bytes;
	int fip_size;
	unsigned int fip_read;
	fip_toc_header_t *toc_header;
	fip_toc_entry_t *toc_entry;

	/* Obtain a handle to the NVM by querying the platform layer */
	plat_get_nvm_handle(&nvm_handle);

#if FWU_BL_TEST
	/* Read the address of backup fip.bin for Firmware Update. */
	ret = io_seek(nvm_handle, IO_SEEK_SET,
			FWU_TFTF_TESTCASE_BUFFER_OFFSET);
	if (ret != IO_SUCCESS)
		return STATUS_FAIL;

	ret = io_read(nvm_handle, (const uintptr_t)&fip_addr,
			sizeof(bytes), &bytes);
	if (ret != IO_SUCCESS)
		return STATUS_FAIL;
#endif /* FWU_BL_TEST */

	/* If the new FIP address is 0 it means no update. */
	if (fip_addr == 0)
		return STATUS_SUCCESS;

	/* Set the ToC Header at the base of the buffer */
	toc_header = (fip_toc_header_t *)fip_addr;

	/* Check if this FIP is Valid */
	if ((toc_header->name != TOC_HEADER_NAME) ||
		(toc_header->serial_number == 0))
		return STATUS_LOAD_ERROR;

	/* Get to the last NULL TOC entry */
	toc_entry = (fip_toc_entry_t *)(toc_header + 1);
	while (!is_uuid_null(&toc_entry->uuid))
		toc_entry++;

	/* get the total size of this FIP */
	fip_size = (int)toc_entry->offset_address;

	/* Copy the new FIP in DDR. */
	memcpy((void *)FIP_IMAGE_TMP_DDR_ADDRESS, (void *)fip_addr, fip_size);

	/* Update the FIP */
	ret = io_seek(nvm_handle, IO_SEEK_SET, 0);
	if (ret != IO_SUCCESS)
		return STATUS_FAIL;

	ret = io_write(nvm_handle, (const uintptr_t)FIP_IMAGE_TMP_DDR_ADDRESS,
			fip_size, &bytes);
	if ((ret != IO_SUCCESS) || fip_size != bytes)
		return STATUS_LOAD_ERROR;

	/* Read the TOC header after update. */
	ret = io_seek(nvm_handle, IO_SEEK_SET, 0);
	if (ret != IO_SUCCESS)
		return STATUS_LOAD_ERROR;

	ret = io_read(nvm_handle, (const uintptr_t)&fip_read,
		sizeof(bytes), &bytes);
	if (ret != IO_SUCCESS)
		return STATUS_FAIL;

	/* Check if this FIP is Valid */
	if (fip_read != TOC_HEADER_NAME)
		return STATUS_LOAD_ERROR;

#if FWU_BL_TEST
	unsigned int done_flag = FIP_IMAGE_UPDATE_DONE_FLAG;
	/* Update the TFTF test case buffer with DONE flag */
	ret = io_seek(nvm_handle, IO_SEEK_SET,
			FWU_TFTF_TESTCASE_BUFFER_OFFSET);
	if (ret != IO_SUCCESS)
		return STATUS_FAIL;

	ret = io_write(nvm_handle, (const uintptr_t)&done_flag,
			4, &bytes);
	if (ret != IO_SUCCESS)
		return STATUS_FAIL;
#endif /* FWU_BL_TEST */

	INFO("FWU Image update success\n");

	return STATUS_SUCCESS;
}

