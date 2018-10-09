/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include "../fvp_def.h"

	.globl	platform_get_core_pos

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
 * ---------------------------------------------------------------------
 */
func platform_get_core_pos
	/*
	 * Check for MT bit in MPIDR. If not set, shift MPIDR to left to make it
	 * look as if in a multi-threaded implementation
	 */
	tst	r0, #MPIDR_MT_MASK
	mov	r3, r0
	lsleq	r3, r0, #MPIDR_AFFINITY_BITS

	/* Extract individual affinity fields from MPIDR */
	ubfx	r0, r3, #MPIDR_AFF0_SHIFT, #MPIDR_AFFINITY_BITS
	ubfx	r1, r3, #MPIDR_AFF1_SHIFT, #MPIDR_AFFINITY_BITS
	ubfx	r2, r3, #MPIDR_AFF2_SHIFT, #MPIDR_AFFINITY_BITS

	/* Compute linear position */
	mov	r3, #FVP_MAX_CPUS_PER_CLUSTER
	mla	r1, r2, r3, r1
	mov	r3, #FVP_MAX_PE_PER_CPU
	mla	r0, r1, r3, r0

	bx	lr
endfunc platform_get_core_pos
