/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <assert_macros.S>

	.globl	zeromem
	.globl	memcpy4
	.globl  disable_mmu_icache

/* -----------------------------------------------------------------------
 * void zeromem(void *mem, unsigned int length);
 *
 * Initialise a memory region to 0.
 * The memory address and length must be 4-byte aligned.
 * -----------------------------------------------------------------------
 */
func zeromem
#if ENABLE_ASSERTIONS
	tst	r0, #0x3
	ASM_ASSERT(eq)
	tst	r1, #0x3
	ASM_ASSERT(eq)
#endif
	add	r2, r0, r1
	mov	r1, #0
z_loop:
	cmp	r2, r0
	beq	z_end
	str	r1, [r0], #4
	b	z_loop
z_end:
	bx	lr
endfunc zeromem

/* --------------------------------------------------------------------------
 * void memcpy4(void *dest, const void *src, unsigned int length)
 *
 * Copy length bytes from memory area src to memory area dest.
 * The memory areas should not overlap.
 * Destination and source addresses must be 4-byte aligned.
 * --------------------------------------------------------------------------
 */
func memcpy4
#if ASM_ASSERTION
	orr	r3, r0, r1
	tst	r3, #0x3
	ASM_ASSERT(eq)
#endif
/* copy 4 bytes at a time */
m_loop4:
	cmp	r2, #4
	blt	m_loop1
	ldr	r3, [r1], #4
	str	r3, [r0], #4
	sub	r2, r2, #4
	b	m_loop4
/* copy byte per byte */
m_loop1:
	cmp	r2,#0
	beq	m_end
	ldrb	r3, [r1], #1
	strb	r3, [r0], #1
	subs	r2, r2, #1
	bne	m_loop1
m_end:
	bx	lr
endfunc memcpy4

/* ---------------------------------------------------------------------------
 * Disable the MMU in Secure State
 * ---------------------------------------------------------------------------
 */

func disable_mmu
	mov	r1, #(HSCTLR_M_BIT | HSCTLR_C_BIT)
do_disable_mmu:
	ldcopr	r0, HSCTLR
	bic	r0, r0, r1
	stcopr	r0, HSCTLR
	isb				// ensure MMU is off
	dsb	sy
	bx	lr
endfunc disable_mmu


func disable_mmu_icache
	ldr	r1, =(HSCTLR_M_BIT | HSCTLR_C_BIT | HSCTLR_I_BIT)
	b	do_disable_mmu
endfunc disable_mmu_icache
