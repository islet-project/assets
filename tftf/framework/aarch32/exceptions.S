/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <tftf.h>

	.globl	tftf_vector

vector_base tftf_vector
	b	tftf_entrypoint
	b	crash_dump		/* Undef */
	b	crash_dump		/* Syscall */
	b	crash_dump		/* Prefetch abort */
	b	crash_dump		/* Data abort */
	b	crash_dump		/* Hyp trap */
	b	tftf_intr_handle	/* IRQ */
	b	crash_dump		/* FIQ */

func crash_dump
	/* Save SP and general-purpose registers on the stack. */
	push	{sp}
	push	{r0-r12, lr}

	/* Print the saved CPU context on the UART. */
	mov	r0, sp
	b	print_exception
endfunc crash_dump

/* ----------------------------------------------------------------------------
 * The IRQ exception handler
 * ----------------------------------------------------------------------------
 */
func tftf_intr_handle
	push	{r0 - r3, lr}
	bl	tftf_irq_handler_dispatcher
	pop	{r0 - r3, lr}
	eret
endfunc tftf_intr_handle
