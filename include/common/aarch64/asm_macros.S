/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __ASM_MACROS_S__
#define __ASM_MACROS_S__

#include <arch.h>
#include <asm_macros_common.S>

#define TLB_INVALIDATE(_type) \
	tlbi	_type

	.macro	func_prologue
	stp	x29, x30, [sp, #-0x10]!
	mov	x29,sp
	.endm

	.macro	func_epilogue
	ldp	x29, x30, [sp], #0x10
	.endm


	.macro	dcache_line_size  reg, tmp
	mrs	\tmp, ctr_el0
	ubfx	\tmp, \tmp, #16, #4
	mov	\reg, #4
	lsl	\reg, \reg, \tmp
	.endm


	.macro	icache_line_size  reg, tmp
	mrs	\tmp, ctr_el0
	and	\tmp, \tmp, #0xf
	mov	\reg, #4
	lsl	\reg, \reg, \tmp
	.endm

	/*
	 * Declare the exception vector table, enforcing it is aligned on a
	 * 2KB boundary, as required by the ARMv8 architecture.
	 * Use zero bytes as the fill value to be stored in the padding bytes
	 * so that it inserts illegal AArch64 instructions. This increases
	 * security, robustness and potentially facilitates debugging.
	 */
	.macro vector_base  label
	.section .vectors, "ax"
	.align 11, 0
	\label:
	.endm

	/*
	 * Create an entry in the exception vector table, enforcing it is
	 * aligned on a 128-byte boundary, as required by the ARMv8
	 * architecture. Use zero bytes as the fill value to be stored in the
	 * padding bytes so that it inserts illegal AArch64 instructions.
	 * This increases security, robustness and potentially facilitates
	 * debugging.
	 */
	.macro vector_entry  label
	.section .vectors, "ax"
	.cfi_sections .debug_frame
	.align 7, 0
	.type \label, %function
	.cfi_startproc
	\label:
	.endm

	/*
	 * Add the bytes until fill the full exception vector, whose size is always
	 * 32 instructions. If there are more than 32 instructions in the
	 * exception vector then an error is emitted.
	 */
	.macro end_vector_entry label
	.cfi_endproc
	.fill	\label + (32 * 4) - .
	.endm

	/*
	 * This macro calculates the base address of an MP stack using the
	 * platform_get_core_pos() index, the name of the stack storage and
	 * the size of each stack
	 * Out: X0 = physical address of stack base
	 * Clobber: X30, X1, X2
	 */
	.macro get_mp_stack _name, _size
	bl  platform_get_core_pos
	ldr x2, =(\_name + \_size)
	mov x1, #\_size
	madd x0, x0, x1, x2
	.endm

	/*
	 * This macro calculates the base address of a UP stack using the
	 * name of the stack storage and the size of the stack
	 * Out: X0 = physical address of stack base
	 */
	.macro get_up_stack _name, _size
	ldr x0, =(\_name + \_size)
	.endm

	/*
	 * Helper macro to generate the best mov/movk combinations according
	 * the value to be moved. The 16 bits from '_shift' are tested and
	 * if not zero, they are moved into '_reg' without affecting
	 * other bits.
	 */
	.macro _mov_imm16 _reg, _val, _shift
		.if (\_val >> \_shift) & 0xffff
			.if (\_val & (1 << \_shift - 1))
				movk	\_reg, (\_val >> \_shift) & 0xffff, LSL \_shift
			.else
				mov	\_reg, \_val & (0xffff << \_shift)
			.endif
		.endif
	.endm

	/*
	 * Helper macro to load arbitrary values into 32 or 64-bit registers
	 * which generates the best mov/movk combinations. Many base addresses
	 * are 64KB aligned the macro will eliminate updating bits 15:0 in
	 * that case
	 */
	.macro mov_imm _reg, _val
		.if (\_val) == 0
			mov	\_reg, #0
		.else
			_mov_imm16	\_reg, (\_val), 0
			_mov_imm16	\_reg, (\_val), 16
			_mov_imm16	\_reg, (\_val), 32
			_mov_imm16	\_reg, (\_val), 48
		.endif
	.endm

	.macro	asm_read_sysreg_el1_or_el2  sysreg
	mrs     x0, CurrentEL
	cmp     x0, #(MODE_EL1 << MODE_EL_SHIFT)
	b.eq    1f
	cmp     x0, #(MODE_EL2 << MODE_EL_SHIFT)
	b.eq    2f
	b       dead
1:
	mrs     x0, \sysreg\()_el1
	b       3f
2:
	mrs     x0, \sysreg\()_el2
3:
	.endm

	.macro	asm_write_sysreg_el1_or_el2  sysreg scratch_reg
	mrs     \scratch_reg, CurrentEL
	cmp     \scratch_reg, #(MODE_EL1 << MODE_EL_SHIFT)
	b.eq    1f
	cmp     \scratch_reg, #(MODE_EL2 << MODE_EL_SHIFT)
	b.eq    2f
	b       dead
1:
	msr     \sysreg\()_el1, x0
	b       3f
2:
	msr     \sysreg\()_el2, x0
3:
	.endm

	.macro asm_read_sctlr_el1_or_el2
	asm_read_sysreg_el1_or_el2 sctlr
	.endm

	.macro asm_write_sctlr_el1_or_el2  scratch_reg
	asm_write_sysreg_el1_or_el2 sctlr \scratch_reg
	.endm

	.macro asm_write_vbar_el1_or_el2  scratch_reg
	asm_write_sysreg_el1_or_el2 vbar \scratch_reg
	.endm

/*
 * Depending on the current exception level, jump to 'label_el1' or 'label_el2'.
 * If the current exception level is neither EL1 nor EL2, jump to 'label_error'
 * instead.
 * The caller needs to provide the macro with a scratch 64-bit register to use.
 * Its contents prior to calling this function will be lost.
 */
	.macro	JUMP_EL1_OR_EL2 scratch_reg, label_el1, label_el2, label_error
	mrs	\scratch_reg, CurrentEL
	cmp	\scratch_reg, #(MODE_EL1 << MODE_EL_SHIFT)
	b.eq	\label_el1
	cmp	\scratch_reg, #(MODE_EL2 << MODE_EL_SHIFT)
	b.eq	\label_el2
	b	\label_error
	.endm

#endif /* __ASM_MACROS_S__ */
