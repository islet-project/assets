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
	and	x1, x0, #MPIDR_CPU_MASK
	and	x0, x0, #MPIDR_CLUSTER_MASK
	/* Swap Cortex-A53/Cortex-A57 order. */
	eor	x0, x0, #(1 << MPIDR_AFF1_SHIFT)
	add	x0, x1, x0, LSR #6
	ret
endfunc platform_get_core_pos
