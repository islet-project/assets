/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Definitions related to the Trusted OS Service as per the SMC Calling
 * Convention.
 */

#ifndef __TRUSTED_OS_H__
#define __TRUSTED_OS_H__

#include <uuid.h>

/* Trusted OS Function IDs that fall under Trusted OS call range */
#define SMC_TOS_CALL_COUNT	0xbf00ff00
#define SMC_TOS_UID		0xbf00ff01
/*				0xbf00ff02 is reserved */
#define SMC_TOS_REVISION	0xbf00ff03

/*
 * Detect whether a Trusted OS is present in the software stack.
 * This is implemented using the Trusted OS UID SMC call.
 *
 * Return 1 if a Trusted OS is detected, else 0. Additionally, the caller may
 * get the UUID of the Trusted OS if he passes a non-null 'tos_uuid' pointer.
 */
unsigned int is_trusted_os_present(uuid_t *tos_uuid);

#endif /* __TRUSTED_OS_H__ */
