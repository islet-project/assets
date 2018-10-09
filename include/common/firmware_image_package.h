/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FIRMWARE_IMAGE_PACKAGE_H__
#define __FIRMWARE_IMAGE_PACKAGE_H__

#include <stdint.h>
#include <uuid.h>

/* This is used as a signature to validate the blob header */
#define TOC_HEADER_NAME	0xAA640001

/* ToC Entry UUIDs */
#define UUID_FIRMWARE_UPDATE_SCP_BL2U \
	{0x03279265, 0x742f, 0x44e6, 0x8d, 0xff, {0x57, 0x9a, 0xc1, 0xff, 0x06, 0x10} }
#define UUID_FIRMWARE_UPDATE_BL2U \
	{0x37ebb360, 0xe5c1, 0x41ea, 0x9d, 0xf3, {0x19, 0xed, 0xa1, 0x1f, 0x68, 0x01} }
#define UUID_FIRMWARE_UPDATE_NS_BL2U \
	{0x111d514f, 0xe52b, 0x494e, 0xb4, 0xc5, {0x83, 0xc2, 0xf7, 0x15, 0x84, 0x0a} }
#define UUID_FIRMWARE_UPDATE_FWU_CERT \
	{0xb28a4071, 0xd618, 0x4c87, 0x8b, 0x2e, {0xc6, 0xdc, 0xcd, 0x50, 0xf0, 0x96} }

typedef struct fip_toc_header {
	uint32_t	name;
	uint32_t	serial_number;
	uint64_t	flags;
} fip_toc_header_t;

typedef struct fip_toc_entry {
	uuid_t		uuid;
	uint64_t	offset_address;
	uint64_t	size;
	uint64_t	flags;
} fip_toc_entry_t;

#endif /* __FIRMWARE_IMAGE_PACKAGE_H__ */
