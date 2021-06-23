/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <sdei.h>


#ifdef __aarch64__
	.globl	inject_serror
	.globl	inject_uncontainable
	.globl	serror_sdei_event_handler

/*
 * Program fault injection register, and wait for ever for the fault to trigger.
 * Note that the model must be launched with fault inject support.
 *
 * x0: Fault record number to program
 * x1: Injected fault properties
 * x2: Type of error to be generated
 * x3: Memory location to wait for, or 0 if no waiting is required
 */
func inject_serror_record
	/* Clear SError received flag if necessary */
	cbz	x3, 1f
	str	xzr, [x3, #0]
	dsb	st
1:
	/* Choose Error record 0 on the PE */
	msr	ERRSELR_EL1, x0
	isb

	/* Enable error reporting */
	orr	x1, x1, #ERXCTLR_ED_BIT
	msr	ERXCTLR_EL1, x1

	/* Program count down timer to 1 */
	mov	x0, #1
	msr	ERXPFGCDN_EL1, x0

	/* Start count down to generate error */
	orr	x2, x2, #ERXPFGCTL_CDEN_BIT
	msr	ERXPFGCTL_EL1, x2
	isb

	/* If no waiting is required, jump to end */
	cbz	x3, 3f

	sevl

2:
	wfe
	dsb	st
	ldr	x0, [x3, #0]
	cbz	x0, 2b

3:
	ret
endfunc inject_serror_record

/*
 * Inject Unrecoverable error through fault record 0. Wait until serror_received
 * is set by the SDEI handler in response to receving the event.
 */
func inject_serror
	/* Inject fault into record 0 */
	mov	x0, #0

	/* Enable error reporting */
	mov	x1, #ERXCTLR_UE_BIT
	msr	ERXCTLR_EL1, x1

	/* Injected fault control */
	mov	x2, #ERXPFGCTL_UEU_BIT

	/* Wait address */
	adrp	x3, serror_received
	add	x3, x3, :lo12:serror_received

	b	inject_serror_record
endfunc inject_serror

/*
 * Inject Uncontainable error through fault record 0. This function doesn't wait
 * as the handling is terminal in EL3.
 */
func inject_uncontainable
	/* Inject fault into record 0 */
	mov	x0, #0

	mov	x1, xzr

	/* Injected fault control */
	mov	x2, #ERXPFGCTL_UC_BIT

	/* Nothing to wait for */
	mov	x3, xzr

	b	inject_serror_record
endfunc inject_uncontainable

/*
 * SDEI event handler for SErrors.
 */
func serror_sdei_event_handler
	stp	x29, x30, [sp, #-16]!
	bl	serror_handler
	ldp	x29, x30, [sp], #16
	mov_imm	x0, SDEI_EVENT_COMPLETE
	mov	x1, xzr
	smc	#0
	b	.
endfunc serror_sdei_event_handler
#endif
