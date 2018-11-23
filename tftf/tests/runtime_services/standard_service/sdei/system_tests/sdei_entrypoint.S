/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <platform_def.h>
#include <sdei.h>

	.globl	sdei_state_entrypoint
	.globl	sdei_entrypoint
	.globl	sdei_entrypoint_resume
	.globl	sdei_handler_done

	.local	event_handled
	.comm	event_handled, PLATFORM_CORE_COUNT * 4, 8

#ifdef AARCH64
func sdei_entrypoint
	stp	xzr, x30, [sp, #-16]!
	bl	sdei_event_handler
	ldp	xzr, x30, [sp],#16
	mov_imm	x0, SDEI_EVENT_COMPLETE
	mov	x1, xzr
	smc	#0
	b	.
endfunc sdei_entrypoint

func sdei_entrypoint_resume
	stp	x2, x30, [sp, #-16]!

	/* Dispatch to C handler */
	bl	sdei_event_handler

	/* Calculate address of event completion variable */
	mrs	x0, mpidr_el1
	mov_imm	x1, MPID_MASK
	and	x0, x0, x1
	bl	platform_get_core_pos
	lsl	x0, x0, #2
	adrp	x1, event_handled
	add	x1, x1, :lo12:event_handled
	add	x1, x0, x1

	/* Mark event handling as complete so `sdei_handler_done` can return */
	mov	w2, #1
	str	w2, [x1]
	sev

	/* Populate `x0` and `x1` to prepare for SMC call */
	ldp	x1, x30, [sp], #16
	mov_imm	x0, SDEI_EVENT_COMPLETE_AND_RESUME
	smc	#0
endfunc sdei_entrypoint_resume

func sdei_handler_done
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp

	/* Calculate address of event completion variable */
	mrs	x0, mpidr_el1
	mov_imm	x1, MPID_MASK
	and	x0, x0, x1
	mov	x29, x30
	bl	platform_get_core_pos
	mov	x30, x29
	lsl	x0, x0, #2
	adrp	x1, event_handled
	add	x1, x1, :lo12:event_handled
	add	x0, x0, x1

again:
	/*
	 * Wait until the timer interrupt fires, which will be handled
	 * as an SDEI event and take us to sdei_entrypoint_resume().
	 */
	wfe
	ldr	w1, [x0]
	cmp	w1, #1
	bne	again

	/* Reset event completion variable for next run */
	mov	w1, #0
	str	w1, [x0]

	ldp	x29, x30, [sp], #16
	ret
endfunc sdei_handler_done

func sdei_state_entrypoint
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	blr	x1

	/* Calculate address of event completion variable */
	mrs	x0, mpidr_el1
	mov_imm	x1, MPID_MASK
	and	x0, x0, x1
	bl	platform_get_core_pos
	lsl	x0, x0, #2
	adrp	x1, event_handled
	add	x1, x1, :lo12:event_handled
	add	x1, x0, x1

	/* Mark event handling as complete so `sdei_handler_done` can return */
	mov	w2, #1
	str	w2, [x1]
	sev

	ldp	x29, x30, [sp],#16
	mov_imm	x0, SDEI_EVENT_COMPLETE
	mov_imm	x1, SDEI_EV_HANDLED
	smc	#0
	b	.
endfunc sdei_state_entrypoint

#else /* AARCH32 */
func sdei_entrypoint
	/* SDEI is not supported on AArch32. */
	b	.
endfunc sdei_entrypoint

func sdei_entrypoint_resume
	/* SDEI is not supported on AArch32. */
	b	.
endfunc sdei_entrypoint_resume

func sdei_handler_done
	/* SDEI is not supported on AArch32. */
	b	.
endfunc sdei_handler_done

func sdei_state_entrypoint
	/* SDEI is not supported on AArch32. */
	b	.
endfunc sdei_state_entrypoint
#endif
