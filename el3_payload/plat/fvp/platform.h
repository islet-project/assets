/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#define PRIMARY_CPU_MPID	0x0

/* Always expect 8 cores, although this is configurable on FVP */
#define CPUS_COUNT		8

#define UART_BASE		0x1c090000

#define FVP_MAX_CPUS_PER_CLUSTER        4
/* Currently multi-threaded CPUs only have a single thread */
#define FVP_MAX_PE_PER_CPU              1

#endif /* __PLATFORM_H__ */
