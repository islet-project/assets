/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
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
#define CACTUS_IMAGE_BASE		ULL(0x1000)
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

#endif /* CACTUS_DEF_H */
