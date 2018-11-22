/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <assert_macros.S>

	.globl	smc

	.globl	zeromem16
	.globl	memcpy16

	.globl	disable_mmu
	.globl	disable_mmu_icache

func smc
	smc	#0
endfunc smc

/* -----------------------------------------------------------------------
 * void zeromem16(void *mem, unsigned int length);
 *
 * Initialise a memory region to 0.
 * The memory address must be 16-byte aligned.
 * -----------------------------------------------------------------------
 */
func zeromem16
#if ENABLE_ASSERTIONS
	tst	x0, #0xf
	ASM_ASSERT(eq)
#endif
	add	x2, x0, x1
/* zero 16 bytes at a time */
z_loop16:
	sub	x3, x2, x0
	cmp	x3, #16
	b.lt	z_loop1
	stp	xzr, xzr, [x0], #16
	b	z_loop16
/* zero byte per byte */
z_loop1:
	cmp	x0, x2
	b.eq	z_end
	strb	wzr, [x0], #1
	b	z_loop1
z_end:
	ret
endfunc zeromem16


/* --------------------------------------------------------------------------
 * void memcpy16(void *dest, const void *src, unsigned int length)
 *
 * Copy length bytes from memory area src to memory area dest.
 * The memory areas should not overlap.
 * Destination and source addresses must be 16-byte aligned.
 * --------------------------------------------------------------------------
 */
func memcpy16
#if ENABLE_ASSERTIONS
	orr	x3, x0, x1
	tst	x3, #0xf
	ASM_ASSERT(eq)
#endif
/* copy 16 bytes at a time */
m_loop16:
	cmp	x2, #16
	b.lt	m_loop1
	ldp	x3, x4, [x1], #16
	stp	x3, x4, [x0], #16
	sub	x2, x2, #16
	b	m_loop16
/* copy byte per byte */
m_loop1:
	cbz	x2, m_end
	ldrb	w3, [x1], #1
	strb	w3, [x0], #1
	subs	x2, x2, #1
	b.ne	m_loop1
m_end:
	ret
endfunc memcpy16

/* ---------------------------------------------------------------------------
 * Disable the MMU at the current exception level (NS-EL1 or EL2)
 * This is implemented in assembler to ensure that the data cache is cleaned
 * and invalidated after the MMU is disabled without any intervening cacheable
 * data accesses
 * ---------------------------------------------------------------------------
 */
func disable_mmu
	mov	x1, #(SCTLR_M_BIT | SCTLR_C_BIT)
do_disable_mmu:
	asm_read_sctlr_el1_or_el2
	bic	x0, x0, x1
	asm_write_sctlr_el1_or_el2 x1
	isb				/* ensure MMU is off */
	mov	x0, #DCCISW	/* DCache clean and invalidate */
	b	dcsw_op_all
endfunc disable_mmu

func disable_mmu_icache
	mov	x1, #(SCTLR_M_BIT | SCTLR_C_BIT | SCTLR_I_BIT)
	b	do_disable_mmu
endfunc disable_mmu_icache

/* Need this label for asm_read/write_sctlr_el1_or_el2 */
dead:
	b	dead
