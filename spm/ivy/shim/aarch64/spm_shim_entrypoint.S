/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <ivy_def.h>
#include <platform_def.h>

	.globl	shim_entrypoint

.section .bss.shim_stacks
	.balign CACHE_WRITEBACK_GRANULE
	.fill	IVY_STACKS_SIZE
shim_stacks_end:

func shim_entrypoint

	/* Setup the stack pointer. */
	adr	x0, shim_stacks_end
	mov	sp, x0

	/* Setup vector base address */
	adr	x0, spm_shim_exceptions_ptr
	msr	vbar_el1, x0
	isb

	/*
	 * Invalidate the data cache for the shim and whole partition.
	 * This prevents re-use of stale data cache entries from prior
	 * bootloader stages.
	 */
	adrp	x0, __SHIM_TEXT_START__
	adrp	x1, __BSS_END__
	sub	x1, x1, x0
	bl	inv_dcache_range

	/* Enable I-Cache */
	mrs	x0, sctlr_el1
	orr	x0, x0, #SCTLR_I_BIT
	msr	sctlr_el1, x0
	isb

	/* Relocate symbols */
shim_pie_fixup:
	ldr	x0, =shim_pie_fixup
	and	x0, x0, #~(0x1000 - 1)
	mov	x1, #IVY_IMAGE_SIZE
	add	x1, x1, x0
	bl	fixup_gdt_reloc

	/* Clear S-EL1 shim BSS */
	adrp	x0, __SHIM_BSS_START__
	adrp	x2, __SHIM_BSS_END__
	sub	x2, x2, x0
	mov	x1, xzr
	bl	memset

	/* Clear S-EL0 partition BSS */
	adrp	x0, __BSS_START__
	adrp	x2, __BSS_END__
	sub	x2, x2, x0
	mov	x1, xzr
	bl	memset

	/* And jump to the C entrypoint. */
	bl	shim_main

	/* Exception return to S-EL0 Ivy application code */
	adrp	x0, ivy_entrypoint
	msr	elr_el1, x0

	/* AArch64 EL0t */
	mov	x0, #((DAIF_FIQ_BIT | DAIF_IRQ_BIT) << SPSR_DAIF_SHIFT)
	msr	spsr_el1, x0

	/* TODO: clear GP/SIMD registers */
	/* TODO: tune EL0 system registers */

	eret

endfunc shim_entrypoint
