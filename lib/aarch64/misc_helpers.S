/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <assert_macros.S>
#include <lib/xlat_tables/xlat_tables_defs.h>

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

/* ---------------------------------------------------------------------------
 * Helper to fixup Global Offset table (GOT) and dynamic relocations
 * (.rela.dyn) at runtime.
 *
 * This function is meant to be used when the firmware is compiled with -fpie
 * and linked with -pie options. We rely on the linker script exporting
 * appropriate markers for start and end of the section. For GOT, we
 * expect __GOT_START__ and __GOT_END__. Similarly for .rela.dyn, we expect
 * __RELA_START__ and __RELA_END__.
 *
 * The function takes the limits of the memory to apply fixups to as
 * arguments (which is usually the limits of the relocable BL image).
 *   x0 -  the start of the fixup region
 *   x1 -  the limit of the fixup region
 * These addresses have to be page (4KB aligned).
 * ---------------------------------------------------------------------------
 */
.globl fixup_gdt_reloc
func fixup_gdt_reloc
	mov	x6, x0
	mov	x7, x1

	/* Test if the limits are 4K aligned */
#if ENABLE_ASSERTIONS
	orr	x0, x0, x1
	tst	x0, #(PAGE_SIZE - 1)
	ASM_ASSERT(eq)
#endif
	/*
	 * Calculate the offset based on return address in x30.
	 * Assume that this function is called within a page at the start of
	 * fixup region.
	 */
	and	x2, x30, #~(PAGE_SIZE - 1)
	sub	x0, x2, x6	/* Diff(S) = Current Address - Compiled Address */

	adrp	x1, __GOT_START__
	add	x1, x1, :lo12:__GOT_START__
	adrp	x2, __GOT_END__
	add	x2, x2, :lo12:__GOT_END__

	/*
	 * GOT is an array of 64_bit addresses which must be fixed up as
	 * new_addr = old_addr + Diff(S).
	 * The new_addr is the address currently the binary is executing from
	 * and old_addr is the address at compile time.
	 */
1:
	ldr	x3, [x1]
	/* Skip adding offset if address is < lower limit */
	cmp	x3, x6
	b.lo	2f
	/* Skip adding offset if address is >= upper limit */
	cmp	x3, x7
	b.ge	2f
	add	x3, x3, x0
	str	x3, [x1]
2:
	add	x1, x1, #8
	cmp	x1, x2
	b.lo	1b

	/* Starting dynamic relocations. Use adrp/adr to get RELA_START and END */
	adrp	x1, __RELA_START__
	add	x1, x1, :lo12:__RELA_START__
	adrp	x2, __RELA_END__
	add	x2, x2, :lo12:__RELA_END__
	/*
	 * According to ELF-64 specification, the RELA data structure is as
	 * follows:
	 *	typedef struct
	 * 	{
	 *		Elf64_Addr r_offset;
	 *		Elf64_Xword r_info;
	 *		Elf64_Sxword r_addend;
	 *	} Elf64_Rela;
	 *
	 * r_offset is address of reference
	 * r_info is symbol index and type of relocation (in this case
	 * 0x403 which corresponds to R_AARCH64_RELATIVE).
	 * r_addend is constant part of expression.
	 *
	 * Size of Elf64_Rela structure is 24 bytes.
	 */
1:
	/* Assert that the relocation type is R_AARCH64_RELATIVE */
#if ENABLE_ASSERTIONS
	ldr	x3, [x1, #8]
	cmp	x3, #0x403
	ASM_ASSERT(eq)
#endif
	ldr	x3, [x1]	/* r_offset */
	add	x3, x0, x3
	ldr	x4, [x1, #16]	/* r_addend */

	/* Skip adding offset if r_addend is < lower limit */
	cmp	x4, x6
	b.lo	2f
	/* Skip adding offset if r_addend entry is >= upper limit */
	cmp	x4, x7
	b.ge	2f

	add	x4, x0, x4	/* Diff(S) + r_addend */
	str	x4, [x3]

2:	add	x1, x1, #24
	cmp	x1, x2
	b.lo	1b

	ret
endfunc fixup_gdt_reloc
