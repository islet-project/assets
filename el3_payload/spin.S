/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "arch.h"
#include "asm_macros.S"
#include "platform.h"

/* Initial value of each entry in the cpus_table[] array */
#define NO_CPU	0xDEADDEADDEADDEAD

/*
 * Declare a per-CPU array to mark the CPUs presence.
 *
 * If cpus_table[i] == NO_CPU then CPU 'i' hasn't successfully booted to the
 * to the EL3 test payload yet.
 *
 * Otherwise, it successfully booted (and cpus_table[i] should contain the
 * CPU MPID).
 */
	.data
	.align 3
cpus_table:
	.rept CPUS_COUNT
	.quad NO_CPU
	.endr


	.text
	.global mark_cpu_presence
	.global is_cpu_present

	/*
	 * void mark_cpu_presence();
	 * Mark the calling CPU present in the CPUs array.
	 * clobbers: x0, x1, x2, x9
	 */
func mark_cpu_presence
	/* Store masked MPID in x2 */
	mrs	x0, mpidr_el1
	ldr	x1, =MPIDR_AFFINITY_MASK
	and	x2, x0, x1

	/* Store core position in x0 */
	mov	x9, x30
	bl	platform_get_core_pos
	mov	x30, x9

	/* Write masked CPU MPID in the CPU entry */
	adr	x1, cpus_table
	add	x1, x1, x0, lsl #3
	str	x2, [x1]

	ret
endfunc mark_cpu_presence

	/*
	 * unsigned int is_cpu_present(unsigned int core_pos);
	 * Return 0 if CPU is absent, 1 if it is present.
	 * clobbers: x0, x1
	 */
func is_cpu_present
	adr	x1, cpus_table
	add	x1, x1, x0, lsl #3
	ldr	x0, [x1]

	ldr	x1, =NO_CPU
	cmp	x0, x1
	b.eq	1f
	mov	x0, #1
	ret
1:
	mov	x0, #0
	ret
endfunc is_cpu_present
