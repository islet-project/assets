/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FWU_NVM_H__
#define __FWU_NVM_H__

#include <nvm.h>
#include <platform_def.h>

#define FIP_IMAGE_UPDATE_DONE_FLAG		(0xDEADBEEF)

#define FWU_TFTF_TESTCASE_BUFFER_OFFSET		\
		(TFTF_NVM_OFFSET + TFTF_STATE_OFFSET(testcase_buffer))

/* Writes the buffer to the flash at offset with length equal to
 * size
 * Returns: STATUS_FAIL, STATUS_SUCCESS, STATUS_OUT_OF_RESOURCES
 */
STATUS fwu_nvm_write(unsigned long long offset, const void *buffer, size_t size);

/* Reads the flash into buffer at offset with length equal to
 * size
 * Returns: STATUS_FAIL, STATUS_SUCCESS, STATUS_OUT_OF_RESOURCES
 */
STATUS fwu_nvm_read(unsigned long long offset, void *buffer, size_t size);

/*
 * This function is used to replace the original fip.bin
 * by the backup fip.bin passed through fip_addr argument.
 */
STATUS fwu_update_fip(unsigned long fip_addr);

#endif /* __FWU_NVM_H__ */
