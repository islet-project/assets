/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef REALM_PLATFORM_H
#define REALM_PLATFORM_H

/*
 * Helper that returns a linear core ID from a MPID
 * Need to provide a RSI_HOST_CALL to request this from Host platform.
 */
unsigned int platform_get_core_pos(u_register_t mpid)
{
	return 0U;
}

#endif /* REALM_PLATFORM_H */
