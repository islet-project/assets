/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CACTUS_DEF_H
#define CACTUS_DEF_H

#include <utils_def.h>

/*
 * Layout of the Secure Partition image.
 */

/* Up to 2 MiB at an arbitrary address that doesn't overlap the devices. */
#define CACTUS_IMAGE_BASE		ULL(0x80000000)
#define CACTUS_IMAGE_SIZE		ULL(0x200000)

/* Memory reserved for stacks */
#define CACTUS_STACKS_SIZE		ULL(0x1000)

/* Memory shared between EL3 and S-EL0 (64 KiB). */
#define CACTUS_SPM_BUF_BASE		(CACTUS_IMAGE_BASE + CACTUS_IMAGE_SIZE)
#define CACTUS_SPM_BUF_SIZE		ULL(0x10000)

/* Memory shared between Normal world and S-EL0 (64 KiB). */
#define CACTUS_NS_BUF_BASE		(CACTUS_SPM_BUF_BASE + CACTUS_SPM_BUF_SIZE)
#define CACTUS_NS_BUF_SIZE		ULL(0x10000)

/* Memory area used by tests (128 KiB). */
#define CACTUS_TEST_MEM_BASE		(CACTUS_NS_BUF_BASE + CACTUS_NS_BUF_SIZE)
#define CACTUS_TEST_MEM_SIZE		ULL(0x20000)

/*
 * UUIDs of Secure Services provided by Cactus
 */

#define CACTUS_SERVICE1_UUID	U(0x01234567), U(0x89ABCDEF), U(0x76543210), U(0xFEDCBA98)
#define CACTUS_SERVICE2_UUID	U(0x0A1B2C3D), U(0x4E5F6789), U(0x55AA00FF), U(0x0F1E2D3C)
#define CACTUS_INVALID_UUID	U(0x1), U(0x2), U(0x3), U(0x4)

#define CACTUS_SERVICE1_UUID_RD	U(0x01234567) U(0x89ABCDEF) U(0x76543210) U(0xFEDCBA98)
#define CACTUS_SERVICE2_UUID_RD	U(0x0A1B2C3D) U(0x4E5F6789) U(0x55AA00FF) U(0x0F1E2D3C)
#define CACTUS_INVALID_UUID_RD	U(0x1) U(0x2) U(0x3) U(0x4)

/*
 * Service IDs
 */

/* Print a magic number unique to Cactus and return */
#define CACTUS_PRINT_MAGIC		U(1)
/* Return a magic number unique to Cactus */
#define CACTUS_GET_MAGIC		U(2)
/* Sleep for a number of milliseconds */
#define CACTUS_SLEEP_MS			U(3)

#define CACTUS_MAGIC_NUMBER		U(0x12481369)

#endif /* CACTUS_DEF_H */
