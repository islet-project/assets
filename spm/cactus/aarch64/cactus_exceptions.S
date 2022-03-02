/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.globl	cactus_vector

/*
 * Exception vector code for unhandled exceptions.
 * Print a crash dump on the UART and loops forever.
 */
.macro unhandled_exception name
	vector_entry \name
	b crash_dump
	end_vector_entry \name
.endm

vector_base cactus_vector

	/*
	 * Current EL with SP0 : 0x0 - 0x200.
	 */
unhandled_exception sync_sp0
unhandled_exception irq_sp0
unhandled_exception fiq_sp0
unhandled_exception serr_sp0

	/*
	 * Current EL with SPx : 0x200 - 0x400.
	 */
vector_entry sync_spx
	b	sync_exception_vector_entry
end_vector_entry sync_spx

vector_entry irq_spx
	b	interrupt_vector_entry
end_vector_entry irq_spx

vector_entry fiq_spx
	b	interrupt_vector_entry
end_vector_entry fiq_spx

unhandled_exception serr_spx

	/*
	 * Lower EL using AArch64 : 0x400 - 0x600.
	 */
unhandled_exception sync_a64
unhandled_exception irq_a64
unhandled_exception fiq_a64
unhandled_exception serr_a64

	/*
	 * Lower EL using AArch32 : 0x600 - 0x800.
	 */
unhandled_exception sync_a32
unhandled_exception irq_a32
unhandled_exception fiq_a32
unhandled_exception serr_a32

.macro save_gp_regs
	stp	x0, x1, [sp, #0x0]
	stp	x2, x3, [sp, #0x10]
	stp	x4, x5, [sp, #0x20]
	stp	x6, x7, [sp, #0x30]
	stp	x8, x9, [sp, #0x40]
	stp	x10, x11, [sp, #0x50]
	stp	x12, x13, [sp, #0x60]
	stp	x14, x15, [sp, #0x70]
	stp	x16, x17, [sp, #0x80]
	stp	x18, x19, [sp, #0x90]
	stp	x20, x21, [sp, #0xa0]
	stp	x22, x23, [sp, #0xb0]
	stp	x24, x25, [sp, #0xc0]
	stp	x26, x27, [sp, #0xd0]
	stp	x28, x29, [sp, #0xe0]
	/* We push xzr simply to keep the stack 16-byte aligned. */
	stp	x30, xzr, [sp, #0xf0]
.endm

.macro restore_gp_regs
	ldp	x30, xzr, [sp, #0xf0]
	ldp	x28, x29, [sp, #0xe0]
	ldp	x26, x27, [sp, #0xd0]
	ldp	x24, x25, [sp, #0xc0]
	ldp	x22, x23, [sp, #0xb0]
	ldp	x20, x21, [sp, #0xa0]
	ldp	x18, x19, [sp, #0x90]
	ldp	x16, x17, [sp, #0x80]
	ldp	x14, x15, [sp, #0x70]
	ldp	x12, x13, [sp, #0x60]
	ldp	x10, x11, [sp, #0x50]
	ldp	x8, x9, [sp, #0x40]
	ldp	x6, x7, [sp, #0x30]
	ldp	x4, x5, [sp, #0x20]
	ldp	x2, x3, [sp, #0x10]
	ldp	x0, x1, [sp, #0x0]
.endm

func sync_exception_vector_entry
	sub	sp, sp, #0x100
	save_gp_regs
	mov	x19, sp
	bl	tftf_sync_exception_handler
	cbnz	x0, 0f
	mov x0, x19
	/* Save original stack pointer value on the stack */
	add	x1, x0, #0x100
	str	x1, [x0, #0xf8]
	b print_exception
0:	restore_gp_regs
	add	sp, sp, #0x100
	eret
endfunc sync_exception_vector_entry

func interrupt_vector_entry
	sub	sp, sp, #0x100
	save_gp_regs
	bl	cactus_interrupt_handler
	restore_gp_regs
	add	sp, sp, #0x100
	eret
endfunc interrupt_vector_entry

func crash_dump
	/* Save general-purpose registers on the stack. */
	sub	sp, sp, #0x100
	save_gp_regs

	/* Save original stack pointer value on the stack. */
	add	x1, sp, #0x100
	str	x1, [sp, #0xf8]

	/* Print the saved CPU context on the UART. */
	mov	x0, sp
	b	print_exception
endfunc crash_dump
