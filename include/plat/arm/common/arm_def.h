/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __ARM_DEF_H__
#define __ARM_DEF_H__

/******************************************************************************
 * Definitions common to all ARM standard platforms
 *****************************************************************************/

/*******************************************************************************
 * Location of the memory buffer shared between Normal World (i.e. TFTF) and the
 * Secure Partition (e.g. Cactus) to pass data associated to secure service
 * requests.
 * Note: This address has to match the one used in TF (see ARM_SP_IMAGE_NS_BUF_*
 * macros).
 ******************************************************************************/
#define ARM_SECURE_SERVICE_BUFFER_BASE	0xff600000ull
#define ARM_SECURE_SERVICE_BUFFER_SIZE	0x10000ull

#endif /* __ARM_DEF_H__ */
