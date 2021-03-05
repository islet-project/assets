/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

#ifndef CACTUS_PLATFORM_DEF_H
#define CACTUS_PLATFORM_DEF_H

#define PLAT_ARM_DEVICE0_BASE		DEVICE0_BASE
#define PLAT_ARM_DEVICE0_SIZE		DEVICE0_SIZE

#define CACTUS_PL011_UART_BASE		PL011_UART2_BASE
#define CACTUS_PL011_UART_CLK_IN_HZ	PL011_UART2_CLK_IN_HZ

#define PLAT_CACTUS_RX_BASE		ULL(0x7300000)

#endif /* CACTUS_PLATFORM_DEF_H */
