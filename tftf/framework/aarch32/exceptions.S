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
	b	.		/* Undef */
	b	.		/* Syscall */
	b	.		/* Prefetch abort */
	b	.		/* Data abort */
	b	.		/* Hyp trap */
	b	tftf_intr_handle/* IRQ */
	b	.		/* FIQ */

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
