/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __ASM_MACROS_S__
#define __ASM_MACROS_S__

#include <arch.h>
#include <asm_macros_common.S>

#define TLB_INVALIDATE(_reg, _coproc) \
	stcopr	_reg, _coproc

	/*
	 * Co processor register accessors
	 */
	.macro ldcopr reg, coproc, opc1, CRn, CRm, opc2
	mrc	\coproc, \opc1, \reg, \CRn, \CRm, \opc2
	.endm

	.macro ldcopr16 reg1, reg2, coproc, opc1, CRm
	mrrc	\coproc, \opc1, \reg1, \reg2, \CRm
	.endm

	.macro stcopr reg, coproc, opc1, CRn, CRm, opc2
	mcr	\coproc, \opc1, \reg, \CRn, \CRm, \opc2
	.endm

	.macro stcopr16 reg1, reg2, coproc, opc1, CRm
	mcrr	\coproc, \opc1, \reg1, \reg2, \CRm
	.endm

	/* Cache line size helpers */
	.macro	dcache_line_size  reg, tmp
	ldcopr	\tmp, CTR
	ubfx	\tmp, \tmp, #16, #4
	mov	\reg, #4
	lsl	\reg, \reg, \tmp
	.endm

	.macro	icache_line_size  reg, tmp
	ldcopr	\tmp, CTR
	and	\tmp, \tmp, #0xf
	mov	\reg, #4
	lsl	\reg, \reg, \tmp
	.endm

	/*
	 * Declare the exception vector table, enforcing it is aligned on a
	 * 32 byte boundary.
	 */
	.macro vector_base  label
	.section .vectors, "ax"
	.align 5
	\label:
	.endm

	/*
	 * This macro calculates the base address of an MP stack using the
	 * platform_get_core_pos() index, the name of the stack storage and
	 * the size of each stack
	 * Out: r0 = physical address of stack base
	 * Clobber: r14, r1, r2
	 */
	.macro get_mp_stack _name, _size
	bl  platform_get_core_pos
	ldr r2, =(\_name + \_size)
	mov r1, #\_size
	mla r0, r0, r1, r2
	.endm

	/*
	 * This macro calculates the base address of a uniprocessor(UP) stack
	 * using the name of the stack storage and the size of the stack
	 * Out: r0 = physical address of stack base
	 */
	.macro get_up_stack _name, _size
	ldr	r0, =(\_name + \_size)
	.endm

#endif /* __ASM_MACROS_S__ */

