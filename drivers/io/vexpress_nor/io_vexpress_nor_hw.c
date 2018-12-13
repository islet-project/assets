/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <mmio.h>
#include <string.h>
#include "io_vexpress_nor_internal.h"
#include "norflash.h"

/* Device Id information */
#define NOR_DEVICE_ID_LOCK_CONFIGURATION          0x02
#define NOR_DEVICE_ID_BLOCK_LOCKED                (1 << 0)
#define NOR_DEVICE_ID_BLOCK_LOCKED_DOWN           (1 << 1)

/* Status Register Bits */
#define NOR_SR_BIT_WRITE                          ((1 << 23) | (1 << 7))
#define NOR_SR_BIT_ERASE                          ((1 << 21) | (1 << 5))
#define NOR_SR_BIT_PROGRAM                        ((1 << 20) | (1 << 4))
#define NOR_SR_BIT_VPP                            ((1 << 19) | (1 << 3))
#define NOR_SR_BIT_BLOCK_LOCKED                   ((1 << 17) | (1 << 1))

/*
 * On chip buffer size for buffered programming operations
 * There are 2 chips, each chip can buffer up to 32 (16-bit)words.
 * Therefore the total size of the buffer is 2 x 32 x 2 = 128 bytes.
 */
#define NOR_MAX_BUFFER_SIZE_IN_BYTES        128
#define NOR_MAX_BUFFER_SIZE_IN_WORDS        (NOR_MAX_BUFFER_SIZE_IN_BYTES / 4)

#define MAX_BUFFERED_PROG_ITERATIONS 1000
#define LOW_16_BITS                  0x0000FFFF
#define FOLD_32BIT_INTO_16BIT(value) ((value >> 16) | (value & LOW_16_BITS))
#define BOUNDARY_OF_32_WORDS         0x7F

#define CHECK_VPP_RANGE_ERROR(status_register, address)			\
		do {							\
			if ((status_register) & NOR_SR_BIT_VPP) {	\
				ERROR("%s (address:0x%X): "		\
				"VPP Range Error\n", __func__, address);\
				err = IO_FAIL;				\
			}						\
		} while (0)

#define CHECK_BLOCK_LOCK_ERROR(status_register, address)		\
	do {								\
		if ((status_register) & NOR_SR_BIT_BLOCK_LOCKED) {	\
			ERROR("%s (address:0x%X): Device Protect "	\
				"Error\n", __func__, address);		\
			err = IO_FAIL;					\
		}							\
	} while (0)

#define CHECK_BLOCK_ERASE_ERROR(status_register, block_offset)		\
	do {								\
		if ((status_register) & NOR_SR_BIT_ERASE) {		\
			ERROR("%s (block_offset=0x%08x:	"		\
				"Block Erase Error status_register"	\
				":0x%x\n",  __func__, block_offset,	\
				status_register);			\
			err = IO_FAIL;					\
		}							\
	} while (0)

#define CHECK_SR_BIT_PROGRAM_ERROR(status_register, address)		\
	do {								\
		if ((status_register) & NOR_SR_BIT_PROGRAM) {		\
			ERROR("%s(address:0x%X): Program Error\n",	\
				__func__, address);			\
			err = IO_FAIL;					\
		}							\
	} while (0)

/* Helper macros to access two flash banks in parallel */
#define NOR_2X16(d)			((d << 16) | (d & 0xffff))

static inline void nor_send_cmd(uintptr_t base_addr, unsigned long cmd)
{
	mmio_write_32(base_addr, NOR_2X16(cmd));
}

static uint32_t flash_read_status(const io_nor_flash_spec_t *device)
{
	/* Prepare to read the status register */
	nor_send_cmd(device->device_address, NOR_CMD_READ_STATUS_REG);

	return mmio_read_32(device->device_address);
}

static uint32_t flash_wait_until_complete(const io_nor_flash_spec_t *device)
{
	uint32_t lock_status;

	/* Wait until the status register gives us the all clear */
	do {
		lock_status = flash_read_status(device);
	} while ((lock_status & NOR_SR_BIT_WRITE) != NOR_SR_BIT_WRITE);

	return lock_status;
}

static int flash_block_is_locked(uint32_t block_offset)
{
	uint32_t lock_status;

	uintptr_t addr = block_offset + (NOR_DEVICE_ID_LOCK_CONFIGURATION << 2);

	/* Send command for reading device id */
	nor_send_cmd(addr, NOR_CMD_READ_ID_CODE);

	/* Read block lock status */
	lock_status = mmio_read_32(addr);

	/* Decode block lock status */
	lock_status = FOLD_32BIT_INTO_16BIT(lock_status);

	if ((lock_status & NOR_DEVICE_ID_BLOCK_LOCKED_DOWN) != 0)
		WARN("flash_block_is_locked: Block LOCKED DOWN\n");

	return lock_status & NOR_DEVICE_ID_BLOCK_LOCKED;
}


static void flash_perform_lock_operation(const io_nor_flash_spec_t *device,
						uint32_t block_offset,
						uint32_t lock_operation)
{
	assert ((lock_operation == NOR_UNLOCK_BLOCK) ||
		(lock_operation == NOR_LOCK_BLOCK));

	/* Request a lock setup */
	nor_send_cmd(block_offset, NOR_CMD_LOCK_UNLOCK);

	/* Request lock or unlock */
	nor_send_cmd(block_offset, lock_operation);

	/* Wait until status register shows device is free */
	flash_wait_until_complete(device);

	/* Put device back into Read Array mode */
	nor_send_cmd(block_offset, NOR_CMD_READ_ARRAY);
}

static void flash_unlock_block_if_necessary(const io_nor_flash_spec_t *device,
						 uint32_t block_offset)
{
	if (flash_block_is_locked(block_offset) != 0)
		flash_perform_lock_operation(device, block_offset,
						NOR_UNLOCK_BLOCK);
}


static int flash_erase_block(const io_nor_flash_spec_t *device,
					  uint32_t block_offset)
{
	int err = IO_SUCCESS;
	uint32_t status_register;

	/* Request a block erase and then confirm it */
	nor_send_cmd(block_offset, NOR_CMD_BLOCK_ERASE);
	nor_send_cmd(block_offset, NOR_CMD_BLOCK_ERASE_ACK);

	/* Wait for the write to complete and then check for any errors;
	 * i.e. check the Status Register */
	status_register = flash_wait_until_complete(device);

	CHECK_VPP_RANGE_ERROR(status_register, block_offset);

	if ((status_register & (NOR_SR_BIT_ERASE | NOR_SR_BIT_PROGRAM)) ==
				(NOR_SR_BIT_ERASE | NOR_SR_BIT_PROGRAM)) {
		ERROR("%s(block_offset=0x%08x: "
			  "Command Sequence Error\n", __func__, block_offset);
		err = IO_FAIL;
	}

	CHECK_BLOCK_ERASE_ERROR(status_register, block_offset);

	CHECK_BLOCK_LOCK_ERROR(status_register, block_offset);

	if (err) {
		/* Clear the Status Register */
		nor_send_cmd(device->device_address, NOR_CMD_CLEAR_STATUS_REG);
	}

	/* Put device back into Read Array mode */
	nor_send_cmd(device->device_address, NOR_CMD_READ_ARRAY);

	return err;
}

/*
 * Writes data to the NOR Flash using the Buffered Programming method.
 *
 * The maximum size of the on-chip buffer is 32-words, because of hardware
 * restrictions. Therefore this function will only handle buffers up to 32
 * words or 128 bytes. To deal with larger buffers, call this function again.
 *
 * This function presumes that both the offset and the offset+BufferSize
 * fit entirely within the NOR Flash. Therefore these conditions will not
 * be checked here.
 *
 * In buffered programming, if the target address is not at the beginning of a
 * 32-bit word boundary, then programming time is doubled and power consumption
 * is increased. Therefore, it is a requirement to align buffer writes to
 * 32-bit word boundaries.
 */
static int flash_write_buffer(const io_nor_flash_spec_t *device,
				uint32_t offset,
				const uint32_t *buffer,
				uint32_t buffer_size)
{
	int err = IO_SUCCESS;
	uint32_t size_in_words;
	uint32_t count;
	volatile uint32_t *data;
	uint32_t timeout;
	int is_buffer_available = 0;
	uint32_t status_register;

	timeout = MAX_BUFFERED_PROG_ITERATIONS;
	is_buffer_available = 0;

	/* Check that the target offset does not cross a 32-word boundary. */
	if ((offset & BOUNDARY_OF_32_WORDS) != 0)
		return IO_FAIL;

	/* This implementation requires the buffer to be 32bit aligned. */
	if (((uintptr_t)buffer & (sizeof(uint32_t) - 1)) != 0)
		return IO_FAIL;

	/* Check there are some data to program */
	assert(buffer_size > 0);

	/* Check that the buffer size does not exceed the maximum hardware
	 * buffer size on chip.
	 */
	assert(buffer_size <= NOR_MAX_BUFFER_SIZE_IN_BYTES);

	/* Check that the buffer size is a multiple of 32-bit words */
	assert((buffer_size % 4) == 0);

	/* Pre-programming conditions checked, now start the algorithm. */

	/* Prepare the data destination address */
	data = (uint32_t *)(uintptr_t)offset;

	/* Check the availability of the buffer */
	do {
		/* Issue the Buffered Program Setup command */
		nor_send_cmd(offset, NOR_CMD_BUFFERED_PROGRAM);

		/* Read back the status register bit#7 from the same offset */
		if (((*data) & NOR_SR_BIT_WRITE) == NOR_SR_BIT_WRITE)
			is_buffer_available = 1;

		/* Update the loop counter */
		timeout--;
	} while ((timeout > 0) && (is_buffer_available == 0));

	/* The buffer was not available for writing */
	if (timeout == 0) {
		err = IO_FAIL;
		goto exit;
	}

	/* From now on we work in 32-bit words */
	size_in_words = buffer_size / sizeof(uint32_t);

	/* Write the word count, which is (buffer_size_in_words - 1),
	 * because word count 0 means one word. */
	nor_send_cmd(offset, size_in_words - 1);

	/* Write the data to the NOR Flash, advancing each address by 4 bytes */
	for (count = 0; count < size_in_words; count++, data++, buffer++)
		*data = *buffer;

	/* Issue the Buffered Program Confirm command, to start the programming
	 * operation */
	nor_send_cmd(device->device_address, NOR_CMD_BUFFERED_PROGRAM_ACK);

	/* Wait for the write to complete and then check for any errors;
	 * i.e. check the Status Register */
	status_register = flash_wait_until_complete(device);

	/* Perform a full status check:
	 * Mask the relevant bits of Status Register.
	 * Everything should be zero, if not, we have a problem */

	CHECK_VPP_RANGE_ERROR(status_register, offset);

	CHECK_SR_BIT_PROGRAM_ERROR(status_register, offset);

	CHECK_BLOCK_LOCK_ERROR(status_register, offset);

	if (err != IO_SUCCESS) {
		/* Clear the Status Register */
		nor_send_cmd(device->device_address,
			     NOR_CMD_CLEAR_STATUS_REG);
	}

exit:
	/* Put device back into Read Array mode */
	nor_send_cmd(device->device_address, NOR_CMD_READ_ARRAY);

	return err;
}

static int flash_write_single_word(const io_nor_flash_spec_t *device,
				int32_t offset, uint32_t data)
{
	int err = IO_SUCCESS;
	uint32_t status_register;

	/* NOR Flash Programming: Request a write single word command */
	nor_send_cmd(offset, NOR_CMD_WORD_PROGRAM);

	/* Store the word into NOR Flash; */
	mmio_write_32(offset, data);

	/* Wait for the write to complete and then check for any errors;
	 * i.e. check the Status Register */
	status_register = flash_wait_until_complete(device);

	/* Perform a full status check: */
	/* Mask the relevant bits of Status Register.
	 * Everything should be zero, if not, we have a problem */

	CHECK_VPP_RANGE_ERROR(status_register, offset);

	CHECK_SR_BIT_PROGRAM_ERROR(status_register, offset);

	CHECK_BLOCK_LOCK_ERROR(status_register, offset);

	if (err != IO_SUCCESS)
		/* Clear the Status Register */
		nor_send_cmd(device->device_address,
					NOR_CMD_CLEAR_STATUS_REG);

	/* Put device back into Read Array mode */
	nor_send_cmd(device->device_address, NOR_CMD_READ_ARRAY);

	return err;
}

int flash_block_write(file_state_t *fp, uint32_t offset,
		const uintptr_t buffer, size_t *written)
{
	int ret;
	uintptr_t buffer_ptr = buffer;
	uint32_t buffer_size;
	uint32_t remaining = fp->block_spec->block_size;
	uint32_t flash_pos = fp->block_spec->region_address + offset;
	uint32_t block_offset = flash_pos;

	/* address passed should be block aligned */
	assert(!(offset % fp->block_spec->block_size));

	/* Unlock block */
	flash_unlock_block_if_necessary(fp->block_spec, block_offset);

	/* Erase one block */
	ret = flash_erase_block(fp->block_spec, flash_pos);

	if (ret != IO_SUCCESS)
		/* Perform lock operation as we unlocked it */
		goto lock_block;

	/* Start by using NOR Flash buffer while the buffer size is a multiple
	 * of 32-bit */
	while ((remaining >= sizeof(uint32_t)) && (ret == IO_SUCCESS)) {
		if (remaining >= NOR_MAX_BUFFER_SIZE_IN_BYTES)
			buffer_size = NOR_MAX_BUFFER_SIZE_IN_BYTES;
		else
			/* Copy the remaining 32bit words of the buffer */
			buffer_size = remaining & (sizeof(uint32_t) - 1);

		ret = flash_write_buffer(fp->block_spec, flash_pos,
				(const uint32_t *)buffer_ptr, buffer_size);
		flash_pos += buffer_size;
		remaining -= buffer_size;
		buffer_ptr += buffer_size;

	}

	/* Write the remaining bytes */
	while ((remaining > 0) && (ret == IO_SUCCESS)) {
		ret = flash_write_single_word(fp->block_spec,
						flash_pos++, buffer_ptr++);
		remaining--;
	}

	if (ret == IO_SUCCESS)
		*written = fp->block_spec->block_size;

lock_block:
	/* Lock the block once done */
	flash_perform_lock_operation(fp->block_spec,
					block_offset,
					NOR_LOCK_BLOCK);

	return ret;
}

/* In case of partial write we need to save the block into a temporary buffer */
static char block_buffer[NOR_FLASH_BLOCK_SIZE];

int flash_partial_write(file_state_t *fp, uint32_t offset,
		const uintptr_t buffer, size_t length, size_t *written)
{
	uintptr_t block_start;
	uint32_t block_size;
	uint32_t block_offset;
	int ret;

	assert((fp != NULL) && (fp->block_spec != NULL));
	assert(written != NULL);

	block_size = fp->block_spec->block_size;
	/* Start address of the block to write */
	block_start = (offset / block_size) * block_size;

	/* Ensure 'block_buffer' is big enough to contain a copy of the block.
	 * 'block_buffer' is reserved at build time - so it might not match  */
	assert(sizeof(block_buffer) >= block_size);

	/*
	 * Check the buffer fits inside a single block.
	 * It must not span several blocks
	 */
	assert((offset / block_size) ==
		  ((offset + length - 1) / block_size));

	/* Make a copy of the block from flash to a temporary buffer */
	memcpy(block_buffer, (void *)(fp->block_spec->region_address +
						block_start), block_size);

	/* Calculate the offset of the buffer into the block */
	block_offset = offset % block_size;

	/* update the content of the block buffer */
	memcpy(block_buffer + block_offset, (void *)buffer, length);

	/* Write the block buffer back */
	ret = flash_block_write(fp, block_start,
					(uintptr_t)block_buffer, written);
	if (ret == IO_SUCCESS)
		*written = length;

	return ret;
}
