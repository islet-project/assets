/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FFA_ENDPOINTS_H
#define FFA_ENDPOINTS_H

#include <platform_def.h>

/* UUID of cactus SPs as defined in the respective manifests. */
#define PRIMARY_UUID {0x1e67b5b4, 0xe14f904a, 0x13fb1fb8, 0xcbdae1da}
#define SECONDARY_UUID {0x092358d1, 0xb94723f0, 0x64447c82, 0xc88f57f5}
#define TERTIARY_UUID {0x735cb579, 0xb9448c1d, 0xe1619385, 0xd2d80a77}

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
