/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FFA_ENDPOINTS_H
#define FFA_ENDPOINTS_H

#include <platform_def.h>

/* UUID of cactus SPs as defined in the respective manifests. */
#define PRIMARY_UUID {0xb4b5671e, 0x4a904fe1, 0xb81ffb13, 0xdae1dacb}
#define SECONDARY_UUID {0xd1582309, 0xf02347b9, 0x827c4464, 0xf5578fc8}
#define TERTIARY_UUID {0x79b55c73, 0x1d8c44b9, 0x859361e1, 0x770ad8d2}

/* vcpu_count of cactus SPs. */
#define PRIMARY_EXEC_CTX_COUNT		PLATFORM_CORE_COUNT
#define SECONDARY_EXEC_CTX_COUNT	PLATFORM_CORE_COUNT
#define TERTIARY_EXEC_CTX_COUNT		(1)
#define IVY_EXEC_CTX_COUNT		(1)

/* UUID of OPTEE SP as defined in the respective manifest. */
#define OPTEE_UUID {0x486178e0, 0xe7f811e3, 0xbc5e0002, 0xa5d5c51b}

#define OPTEE_FFA_GET_API_VERSION	(0)
#define OPTEE_FFA_GET_OS_VERSION	(1)
#define OPTEE_FFA_GET_OS_VERSION_MAJOR	(3)
#define OPTEE_FFA_GET_OS_VERSION_MINOR	(10)

#endif
