/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QUARK_DEF_H
#define QUARK_DEF_H

#include <utils_def.h>

/*
 * Layout of the Secure Partition image.
 */

/* The base address is 0 to reduce the address space size */
#define QUARK_IMAGE_BASE		ULL(0x00000000)
#define QUARK_IMAGE_SIZE		ULL(0x10000)

/* Memory reserved for stacks */
#define QUARK_STACKS_SIZE		ULL(0x1000)

/* Memory shared between EL3 and S-EL0 (64 KiB). */
#define QUARK_SPM_BUF_BASE		(QUARK_IMAGE_BASE + QUARK_IMAGE_SIZE)
#define QUARK_SPM_BUF_SIZE		ULL(0x10000)

/*
 * UUIDs of Secure Services provided by Quark
 */

/* Mass (MeV/c^2): Up, down, charm, strange */
#define QUARK_SERVICE1_UUID	U(0x23), U(0x48), U(0x1275), U(0x95)

#define QUARK_SERVICE1_UUID_RD	U(0x23) U(0x48) U(0x1275) U(0x95)

/*
 * Service IDs
 */
/* Return a magic number unique to QUARK */
#define QUARK_GET_MAGIC		U(2002)

/* Mass (MeV/c^2): Top */
#define QUARK_MAGIC_NUMBER	U(0x173210)

#endif /* QUARK_DEF_H */
