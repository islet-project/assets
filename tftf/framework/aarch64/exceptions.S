/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.globl	tftf_vector

/*
 * Exception vector code for unhandled exceptions.
 * Print a crash dump on the UART and loops forever.
 */
.macro unhandled_exception name
	vector_entry \name
	b crash_dump
	end_vector_entry \name
.endm

vector_base tftf_vector

	/*
	 * Current EL with SP0 : 0x0 - 0x200.
	 */
unhandled_exception sync_exception_sp_el0
unhandled_exception irq_sp_el0
unhandled_exception fiq_sp_el0
unhandled_exception serror_sp_el0

	/*
	 * Current EL with SPx : 0x200 - 0x400.
	 */
unhandled_exception sync_exception_sp_elx

vector_entry irq_sp_elx
	b	irq_vector_entry
end_vector_entry irq_sp_elx

unhandled_exception fiq_sp_elx
unhandled_exception serror_sp_elx

	/*
	 * Lower EL using AArch64 : 0x400 - 0x600.
	 */
unhandled_exception sync_exception_aarch64
unhandled_exception irq_aarch64
unhandled_exception fiq_aarch64
unhandled_exception serror_aarch64

	/*
	 * Lower EL using AArch32 : 0x600 - 0x800.
	 */
unhandled_exception sync_exception_aarch32
unhandled_exception irq_aarch32
unhandled_exception fiq_aarch32
unhandled_exception serror_aarch32

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

func irq_vector_entry
	sub	sp, sp, #0x100
	save_gp_regs
	bl	tftf_irq_handler_dispatcher
	restore_gp_regs
	add	sp, sp, #0x100
	eret
endfunc irq_vector_entry

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
