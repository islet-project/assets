/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLAT_ARM_H__
#define __PLAT_ARM_H__

/*
 * Initialises the IO
 * Returns: IO_SUCCESS
 *          IO_FAIL
 *          IO_NOT_SUPPORTED
 *          IO_RESOURCES_EXHAUSTED
 */
int arm_io_setup(void);

/* Initialises the IO and the GIC. */
void arm_platform_setup(void);

/*******************************************************************************
 * ARM platforms porting interfaces are located below.
 ******************************************************************************/

/* Initialises the Generic Interrupt Controller (GIC). */
void plat_arm_gic_init(void);

#endif /* __PLAT_ARM_H__ */
