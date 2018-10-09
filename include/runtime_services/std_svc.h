/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Definitions related to the Standard Service as per the SMC Calling Convention
 *
 * Although PSCI calls are part of the Standard Service call range, PSCI-related
 * definitions are not in this header file but in psci.h instead.
 */

#ifndef __STD_SVC_H__
#define __STD_SVC_H__

/* SMC function IDs for Standard Service queries */
#define SMC_STD_SVC_CALL_COUNT		0x8400ff00
#define SMC_STD_SVC_UID			0x8400ff01
/*					0x8400ff02 is reserved */
#define SMC_STD_SVC_REVISION		0x8400ff03

/* Standard Service Calls revision numbers */
#define STD_SVC_REVISION_MAJOR		0x0
#define STD_SVC_REVISION_MINOR		0x1

#endif /* __STD_SVC_H__ */
