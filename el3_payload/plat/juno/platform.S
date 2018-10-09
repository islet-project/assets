/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "arch.h"
#include "asm_macros.S"

	.text
	.global platform_get_core_pos

	/* --------------------------------------------------------------------
	 * unsigned int get_core_pos(uint64_t mpidr);
	 *
	 * Helper function to calculate the core position from its MPID.
	 * Core positions must be consecutive, there must be no holes.
	 *
	 * MPID  -> core position:
	 * 0x100 -> 0
	 * 0x101 -> 1
	 * 0x102 -> 2
	 * 0x103 -> 3
	 *   0x0 -> 4
	 *   0x1 -> 5
	 * --------------------------------------------------------------------
	 */
func platform_get_core_pos
	and	x1, x0, #MPIDR_CPU_MASK
	and	x0, x0, #MPIDR_CLUSTER_MASK
	eor	x0, x0, #(1 << MPIDR_AFFINITY_BITS)  /* swap cluster order */
	add	x0, x1, x0, LSR #6
	ret
endfunc platform_get_core_pos
