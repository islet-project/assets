/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros_common.S>

#define COOKIE_SIZE			20

#ifdef AARCH64
/* int do_state_switch(void *) */
	.globl do_state_switch
func do_state_switch
	/* Temporarily save beginning of stack */
	mov	x7, sp

	/*
	 * When asking to switch execution state, we can't expect general
	 * purpose registers hold their values. EL3 might clear them all; even
	 * if EL3 were to preserve them, the register width shrinks and then
	 * expands, leaving the upper part unknown. So save them before and
	 * restore after call to switch.
	 */
	stp	x8, x9, [sp, #-16]!
	stp	x10, x11, [sp, #-16]!
	stp	x12, x13, [sp, #-16]!
	stp	x14, x15, [sp, #-16]!
	stp	x16, x17, [sp, #-16]!
	stp	x18, x19, [sp, #-16]!
	stp	x20, x21, [sp, #-16]!
	stp	x22, x23, [sp, #-16]!
	stp	x24, x25, [sp, #-16]!
	stp	x26, x27, [sp, #-16]!
	stp	x28, x29, [sp, #-16]!

	/*
	 * State switch effectively means a soft reset; so SCTLR will lose its
	 * value too.
	 */
	mrs	x1, CurrentEL
	cmp	x1, #(2 << 2)
	b.ne	1f
	mrs	x1, sctlr_el2
	b	2f
1:
	mrs	x1, sctlr_el1
2:
	stp	x30, x1, [sp, #-16]!

	/* Store the PC in the cookie when switching back to AArch64 */
	ldr	x4, =state_switch_cookie
	adr	x2, do_switch_back
	mov	w1, w2
	lsr	x1, x1, #32
	str	w1, [x4, #0]	/* PC hi */
	str	w2, [x4, #4]	/* PC lo */

	/* Store valid stack pointer in cookie */
	mov	x8, sp
	str	x8, [x4, #8]

	/* Stash stack and LR before calling functions */
	mov	x28, x7
	mov	x29, x30

	mov	x10, x0

	/*
	 * Clean and invalidate cookie memory as it's going to be accessed with
	 * MMU off in the new state.
	 */
	mov	x0, x4
	ldr	x1, =COOKIE_SIZE
	bl	flush_dcache_range

	/*
	 * Flush stack context saved on stack as it'll be accessed immediately
	 * after switching back, with MMU off.
	 */
	mov	x0, x8
	sub	x1, x28, x8
	bl	flush_dcache_range

	/* Prepare arguments for state switch SMC */
	ldr	x0, [x10], #8
	ldr	x1, [x10], #8
	ldr	x2, [x10], #8
	ldr	x3, [x10], #8
	ldr	x4, [x10], #8

	/* Switch state */
	smc	#0

	/*
	 * We reach here only if the SMC failed. If so, restore previously
	 * modified callee-saved registers, rewind stack, and return to caller
	 * with the error code from SMC.
	 */
	 mov	x1, x28
	 mov	x2, x29
	 ldp	x28, x29, [sp, #16]
	 mov	sp, x1
	 ret	x2

restore_context:
	/* Restore context */
	ldp	x30, x1, [sp], #16
	ldp	x28, x29, [sp], #16
	ldp	x26, x27, [sp], #16
	ldp	x24, x25, [sp], #16
	ldp	x22, x23, [sp], #16
	ldp	x20, x21, [sp], #16
	ldp	x18, x19, [sp], #16
	ldp	x16, x17, [sp], #16
	ldp	x14, x15, [sp], #16
	ldp	x12, x13, [sp], #16
	ldp	x10, x11, [sp], #16
	ldp	x8, x9, [sp], #16

	dsb	sy
	mrs	x0, CurrentEL
	cmp	x0, #(2 << 2)
	b.ne	1f
	msr	sctlr_el2, x1
	b	2f
1:
	msr	sctlr_el1, x1
2:
	isb

	mov	x0, #0
	ret
endfunc do_state_switch

/* AArch64 entry point when switching back from AArch32 */
do_switch_back:
	/* w0 and w1 have the cookie */
	lsl	x0, x0, #32
	orr	x0, x1, x0

	ldr	x1, [x0, #8]
	mov	sp, x1

	b	restore_context

	.section .data, "aw"

/* AArch32 instructions to switch state back to AArch64, stored as data */
	.align	2
	.globl	state_switch_a32_entry
state_switch_a32_entry:
	/* Use the same context when switching back */
	.word	0xe1a03000	/* mov	r3, r0 */
	.word	0xe1a04001	/* mov	r4, r1 */

	/* Set success flag in cookie */
	.word	0xe3a00001	/* mov	r0, #1 */
	.word	0xe5810010	/* str	r0, [r1, #16] */

	/* Setup arguments for SMC */
	.word	0xe3a00020	/* mov	r0, #0x0020 */
	.word	0xe3480200	/* movt	r0, #0x8200 */

	.word	0xe5912004	/* ldr	r2, [r1, #4] */
	.word	0xe5911000	/* ldr	r1, [r1, #0] */
	.word	0xe1600070	/* smc	#0x0 */
	.word	0xeafffffe      /* b	. */

#else /* !AARCH64 */

/* Not supported on AArch32 yet */
func do_state_switch
	mov	r0, #-1
	bx	lr
endfunc do_state_switch

#endif /* AARCH64 */
