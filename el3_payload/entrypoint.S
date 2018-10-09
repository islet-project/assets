/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "arch.h"
#include "asm_macros.S"
#include "platform.h"

#define EOT_ASCII_CODE		4

	.data
welcome_str:
	.asciz "Booting the EL3 test payload\r\n"
all_cpus_booted_str:
	.asciz "All CPUs booted!\r\n"

	.text
	.global entrypoint

func entrypoint
	bl	mark_cpu_presence

	/* Distinguish primary from secondary CPUs */
	mrs	x0, mpidr_el1
	ldr	x1, =MPIDR_AFFINITY_MASK
	and	x0, x0, x1

	ldr	x1, =PRIMARY_CPU_MPID
	cmp	x0, x1
	b.ne	spin_forever

	/*
	 * Only the primary CPU executes the code below
	 */

	adr	x0, welcome_str
	bl	print_string

	/* Wait to see each CPU */
	mov	x3, xzr
1:
	mov	x0, x3
	bl	is_cpu_present
	cbz	x0, 1b

	/* Next CPU, if any */
	add	x3, x3, #1
	mov	x0, #CPUS_COUNT
	cmp	x3, x0
	b.lt	1b

	/* All CPUs have been detected, announce the good news! */
	adr	x0, all_cpus_booted_str
	bl	print_string

	/* Send EOT (End of Transmission character) character over the UART */
	mov	x0, #EOT_ASCII_CODE
	bl	print_char

spin_forever:
	wfe
	b	spin_forever
endfunc entrypoint
