/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.globl	tftf_vector

vector_base tftf_vector

	/*
	 * Current EL with SP0 : 0x0 - 0x200.
	 */
vector_entry SynchronousExceptionSP0
	b	SynchronousExceptionSP0
end_vector_entry SynchronousExceptionSP0

vector_entry IrqSP0
	b	IrqSP0
end_vector_entry IrqSP0

vector_entry FiqSP0
	b	FiqSP0
end_vector_entry FiqSP0

vector_entry SErrorSP0
	b	SErrorSP0
end_vector_entry SErrorSP0

	/*
	 * Current EL with SPx : 0x200 - 0x400.
	 */
vector_entry SynchronousExceptionSPx
	b	SynchronousExceptionSPx
end_vector_entry SynchronousExceptionSPx

vector_entry IrqSPx
	b	irq_vector_entry
end_vector_entry IrqSPx

vector_entry FiqSPx
	b	FiqSPx
end_vector_entry FiqSPx

vector_entry SErrorSPx
	b	SErrorSPx
end_vector_entry SErrorSPx

	/*
	 * Lower EL using AArch64 : 0x400 - 0x600.
	 */
vector_entry SynchronousExceptionA64
	b	SynchronousExceptionA64
end_vector_entry SynchronousExceptionA64

vector_entry IrqA64
	b	IrqA64
end_vector_entry IrqA64

vector_entry FiqA64
	b	FiqA64
end_vector_entry FiqA64

vector_entry SErrorA64
	b	SErrorA64
end_vector_entry SErrorA64

	/*
	 * Lower EL using AArch32 : 0x600 - 0x800.
	 */
vector_entry SynchronousExceptionA32
	b	SynchronousExceptionA32
end_vector_entry SynchronousExceptionA32

vector_entry IrqA32
	b	IrqA32
end_vector_entry IrqA32

vector_entry FiqA32
	b	FiqA32
end_vector_entry FiqA32

vector_entry SErrorA32
	b	SErrorA32
end_vector_entry SErrorA32

/*
 * Exceptions will always be from the same exception level so no need to save
 * and restore SPSR.
 */
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
	mrs     x0, sp_el0
	stp	x30, x0, [sp, #0xf0]
.endm

.macro restore_gp_regs
	ldp	x30, x0, [sp, #0xf0]
	msr	sp_el0, x0
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
