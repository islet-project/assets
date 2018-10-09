/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __IO_FIP_H__
#define __IO_FIP_H__

#include <assert.h>

struct io_dev_connector;

int register_io_dev_fip(const struct io_dev_connector **dev_con);

enum {
	/* Non-Trusted Updater Firmware NS_BL1U */
	NS_BL1U_IMAGE_ID = 16,

	/* Trusted FWU Certificate */
	FWU_CERT_ID,

	/* SCP Firmware SCP_BL2U */
	SCP_BL2U_IMAGE_ID,

	/* Trusted Updater Firmware BL2U */
	BL2U_IMAGE_ID,

	/* Non-Trusted Updater Firmware NS_BL2U */
	NS_BL2U_IMAGE_ID,

	/* FWU Firmware Image Package */
	FWU_FIP_IMAGE_ID
};

static inline const char *get_image_name(unsigned int image_id)
{
	static const char *image_names[] = {
		"Non-Trusted Updater Firmware (NS_BL1U)",
		"Trusted FWU Certificate",
		"SCP Firmware (SCP_BL2U)",
		"Trusted Updater Firmware (BL2U)",
		"Non-Trusted Updater Firmware (NS_BL2U)"
		"FWU Firmware Image Package",
	};
	assert((image_id >= NS_BL1U_IMAGE_ID) && (image_id <= FWU_FIP_IMAGE_ID));
	return image_names[image_id - NS_BL1U_IMAGE_ID];
}

#endif /* __IO_FIP_H__ */
