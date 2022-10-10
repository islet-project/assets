/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef HOST_REALM_MEM_LAYOUT_H
#define HOST_REALM_MEM_LAYOUT_H

#include <realm_def.h>

#include <platform_def.h>

/*
 * Realm payload Memory Usage Layout
 *
 * +--------------------------+     +---------------------------+
 * |                          |     | Host Image                |
 * |    TFTF                  |     | (TFTF_MAX_IMAGE_SIZE)     |
 * | Normal World             | ==> +---------------------------+
 * |    Image                 |     | Realm Image               |
 * | (MAX_NS_IMAGE_SIZE)      |     | (REALM_MAX_LOAD_IMG_SIZE  |
 * +--------------------------+     +---------------------------+
 * |  Memory Pool             |     | Heap Memory               |
 * | (NS_REALM_SHARED_MEM_SIZE|     | (PAGE_POOL_MAX_SIZE)      |
 * |  + PAGE_POOL_MAX_SIZE)   | ==> |                           |
 * |                          |     |                           |
 * |                          |     +---------------------------+
 * |                          |     | Shared Region             |
 * |                          |     | (NS_REALM_SHARED_MEM_SIZE)|
 * +--------------------------+     +---------------------------+
 *
 */

/*
 * Default values defined in platform.mk, and can be provided as build arguments
 * TFTF_MAX_IMAGE_SIZE: 1mb
 */

#ifdef TFTF_MAX_IMAGE_SIZE
/* 1MB for shared buffer between Realm and Host*/
 #define NS_REALM_SHARED_MEM_SIZE	U(0x100000)
/* 3MB of memory used as a pool for realm's objects creation*/
 #define PAGE_POOL_MAX_SIZE		U(0x300000)
/* Base address of each section */
 #define REALM_IMAGE_BASE		(TFTF_BASE + TFTF_MAX_IMAGE_SIZE)
 #define PAGE_POOL_BASE			(REALM_IMAGE_BASE + REALM_MAX_LOAD_IMG_SIZE)
 #define NS_REALM_SHARED_MEM_BASE	(PAGE_POOL_BASE + PAGE_POOL_MAX_SIZE)
#else
 #define NS_REALM_SHARED_MEM_SIZE	0U
 #define PAGE_POOL_MAX_SIZE		0U
 #define TFTF_MAX_IMAGE_SIZE		DRAM_SIZE
/* Base address of each section */
 #define REALM_IMAGE_BASE		0U
 #define PAGE_POOL_BASE			0U
 #define NS_REALM_SHARED_MEM_BASE	0U
#endif

#endif /* HOST_REALM_MEM_LAYOUT_H */
