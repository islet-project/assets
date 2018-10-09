/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FWU_NVM_H__
#define __FWU_NVM_H__

#include <nvm.h>
#include <platform_def.h>

#define FIP_IMAGE_UPDATE_DONE_FLAG		(0xDEADBEEF)
/*
 * This is the temporary ddr address for loading backup fip.bin
 * image from NVM which is used for replacing original fip.bin
 * This address is chosen such that the NS_BL2U can be expanded
 * in future and also considering the large size of fip.bin.
 */
#define FIP_IMAGE_TMP_DDR_ADDRESS		(DRAM_BASE + 0x100000)
#define FWU_TFTF_TESTCASE_BUFFER_OFFSET		\
		(TFTF_NVM_OFFSET + TFTF_STATE_OFFSET(testcase_buffer))

/*
 * This offset is used to corrupt data in fip.bin
 * The offset is from the base where fip.bin is
 * located in NVM. This particular value is chosen
 * to make sure the corruption is done beyond fip header.
 */
#define FIP_CORRUPT_OFFSET		(0x300)

/*
 * This is the base address for backup fip.bin image in NVM
 * which is used for replacing original fip.bin
 * This address is chosen such that it can stay with all
 * the other images in the NVM.
 */
#define FIP_BKP_ADDRESS			(FLASH_BASE + 0x1000000)

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
