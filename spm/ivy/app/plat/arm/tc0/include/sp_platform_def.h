/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains common defines for a secure partition. The correct
 * platform_def.h header file is selected according to the secure partition
 * and platform being built using the make scripts.
 */

#ifndef SP_PLATFORM_DEF_H
#define SP_PLATFORM_DEF_H

#define PLAT_SP_RX_BASE			ULL(0x7300000)

#endif /* SP_PLATFORM_DEF_H */
