/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

#ifndef CACTUS_PLATFORM_DEF_H
#define CACTUS_PLATFORM_DEF_H

#define PLAT_ARM_DEVICE0_BASE		TC0_DEVICE0_BASE
#define PLAT_ARM_DEVICE0_SIZE		TC0_DEVICE0_SIZE

#define CACTUS_PL011_UART_BASE		PL011_UART1_BASE
#define CACTUS_PL011_UART_CLK_IN_HZ	PL011_UART1_CLK_IN_HZ

#define PLAT_CACTUS_RX_BASE		ULL(0xfe300000)
#define PLAT_CACTUS_CORE_COUNT		(8)

#define CACTUS_PRIMARY_EC_COUNT		(8U)
#define CACTUS_SECONDARY_EC_COUNT	(8U)
#define CACTUS_TERTIARY_EC_COUNT	(1U)

/* Scratch memory used for SMMUv3 driver testing purposes in Cactus SP */
/* SMMUv3 tests are disabled for TC platform */
#define PLAT_CACTUS_MEMCPY_BASE		ULL(0xfe400000)
#define PLAT_CACTUS_MEMCPY_RANGE	ULL(0x8000)

/* Base address of user and PRIV frames in SMMUv3TestEngine */
/* SMMUv3 tests are disabled for TC platform */
#define USR_BASE_FRAME			ULL(0x0)
#define PRIV_BASE_FRAME			ULL(0x0)

#endif /* CACTUS_PLATFORM_DEF_H */
