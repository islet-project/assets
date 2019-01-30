/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SMCCC_H__
#define __SMCCC_H__

#include <utils_def.h>

#define SMCCC_VERSION_MAJOR_SHIFT	U(16)
#define SMCCC_VERSION_MAJOR_MASK	U(0x7FFF)
#define SMCCC_VERSION_MINOR_SHIFT	U(0)
#define SMCCC_VERSION_MINOR_MASK	U(0xFFFF)
#define MAKE_SMCCC_VERSION(_major, _minor) \
	((((uint32_t)(_major) & SMCCC_VERSION_MAJOR_MASK) << \
						SMCCC_VERSION_MAJOR_SHIFT) \
	| (((uint32_t)(_minor) & SMCCC_VERSION_MINOR_MASK) << \
						SMCCC_VERSION_MINOR_SHIFT))

#define SMC_UNKNOWN			-1

/*******************************************************************************
 * Bit definitions inside the function id as per the SMC calling convention
 ******************************************************************************/
#define FUNCID_TYPE_SHIFT		31
#define FUNCID_CC_SHIFT			30
#define FUNCID_OEN_SHIFT		24
#define FUNCID_NUM_SHIFT		0

#define FUNCID_TYPE_MASK		0x1
#define FUNCID_CC_MASK			0x1
#define FUNCID_OEN_MASK			0x3f
#define FUNCID_NUM_MASK			0xffff

#define FUNCID_TYPE_WIDTH		1
#define FUNCID_CC_WIDTH			1
#define FUNCID_OEN_WIDTH		6
#define FUNCID_NUM_WIDTH		16

#define SMC_64				1
#define SMC_32				0
#define SMC_TYPE_FAST			1
#define SMC_TYPE_STD			0

/*******************************************************************************
 * Owning entity number definitions inside the function id as per the SMC
 * calling convention
 ******************************************************************************/
#define OEN_ARM_START			0
#define OEN_ARM_END			0
#define OEN_CPU_START			1
#define OEN_CPU_END			1
#define OEN_SIP_START			2
#define OEN_SIP_END			2
#define OEN_OEM_START			3
#define OEN_OEM_END			3
#define OEN_STD_START			4	/* Standard Calls */
#define OEN_STD_END			4
#define OEN_TAP_START			48	/* Trusted Applications */
#define OEN_TAP_END			49
#define OEN_TOS_START			50	/* Trusted OS */
#define OEN_TOS_END			63
#define OEN_LIMIT			64

#endif /* __SMCCC_H__ */
