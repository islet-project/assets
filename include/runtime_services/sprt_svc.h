/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SPRT_SVC_H__
#define __SPRT_SVC_H__

#include <smccc.h>
#include <utils_def.h>

/* SPRT_VERSION helpers */
#define SPRT_VERSION_MAJOR		U(0)
#define SPRT_VERSION_MAJOR_SHIFT	16
#define SPRT_VERSION_MAJOR_MASK		U(0x7FFF)
/* TODO: Move up minor version to 1 when SPRT is properly supported. */
#define SPRT_VERSION_MINOR		U(0)
#define SPRT_VERSION_MINOR_SHIFT	0
#define SPRT_VERSION_MINOR_MASK		U(0xFFFF)
#define SPRT_VERSION_FORM(major, minor)	((((major) & SPRT_VERSION_MAJOR_MASK) << SPRT_VERSION_MAJOR_SHIFT) | \
					((minor) & SPRT_VERSION_MINOR_MASK))
#define SPRT_VERSION_COMPILED		SPRT_VERSION_FORM(SPRT_VERSION_MAJOR, SPRT_VERSION_MINOR)

/* TODO: Check all values below are correct when they're specified in SPRT. */

/* SPRT function IDs */
#define SPRT_FID_VERSION		U(0x0)
#define SPRT_FID_RETURN_RESPONSE	U(0x1)

#define SPRT_FID_MASK			U(0xFF)

/* Definitions to build the complete SMC ID */
#define SPRT_SMC_ID(sprt_fid)		((FUNCID_SERV_SPRT << FUNCID_SERV_SHIFT) | \
					 (U(1) << 31) | ((sprt_fid) & SPRT_FID_MASK))

/* Complete SMC IDs */
#define SPRT_VERSION			SPRT_SMC_ID(SPRT_FID_VERSION)
#define SPRT_RETURN_RESPONSE		SPRT_SMC_ID(SPRT_FID_RETURN_RESPONSE)

/* SPRT error codes. */
#define SPRT_SUCCESS		 0
#define SPRT_NOT_SUPPORTED	-1
#define SPRT_INVALID_PARAMETER	-2

#endif /* __SPRT_SVC_H__ */
