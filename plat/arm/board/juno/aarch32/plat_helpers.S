/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>

	.globl	platform_get_core_pos

	/*
	 * Return 0 to 3 for the Cortex-A53 cores and 4 to 5 for the Cortex-A57
	 * cores.
	 */
func platform_get_core_pos
	and	r1, r0, #MPIDR_CPU_MASK
	and	r0, r0, #MPIDR_CLUSTER_MASK
	/* Swap Cortex-A53/Cortex-A57 order. */
	eor	r0, r0, #(1 << MPIDR_AFF1_SHIFT)
	add	r0, r1, r0, LSR #6
	bx	lr
endfunc platform_get_core_pos
