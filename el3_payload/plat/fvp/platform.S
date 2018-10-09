/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "arch.h"
#include "asm_macros.S"
#include "platform.h"

	.text
	.global platform_get_core_pos

/*----------------------------------------------------------------------
 * unsigned int platform_get_core_pos(unsigned long mpid)
 *
 * Function to calculate the core position on FVP.
 *
 * (ClusterId * FVP_MAX_CPUS_PER_CLUSTER * FVP_MAX_PE_PER_CPU) +
 * (CPUId * FVP_MAX_PE_PER_CPU) +
 * ThreadId
 *
 * which can be simplified as:
 *
 * ((ClusterId * FVP_MAX_CPUS_PER_CLUSTER + CPUId) * FVP_MAX_PE_PER_CPU)
 * + ThreadId
 *
 * clobbers: x0, x1, x3, x4
 * ---------------------------------------------------------------------
 */
func platform_get_core_pos
	/*
	 * Check for MT bit in MPIDR. If not set, shift MPIDR to left to make it
	 * look as if in a multi-threaded implementation.
	 */
	tst	x0, #MPIDR_MT_MASK
	lsl	x3, x0, #MPIDR_AFFINITY_BITS
	csel	x3, x3, x0, eq

	/* Extract individual affinity fields from MPIDR */
	ubfx	x0, x3, #MPIDR_AFF0_SHIFT, #MPIDR_AFFINITY_BITS
	ubfx	x1, x3, #MPIDR_AFF1_SHIFT, #MPIDR_AFFINITY_BITS
	ubfx	x4, x3, #MPIDR_AFF2_SHIFT, #MPIDR_AFFINITY_BITS

	/* Compute linear position */
	mov	x3, #FVP_MAX_CPUS_PER_CLUSTER
	madd	x1, x4, x3, x1
	mov	x3, #FVP_MAX_PE_PER_CPU
	madd	x0, x1, x3, x0
	ret
endfunc platform_get_core_pos
