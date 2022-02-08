/*
 * Copyright (c) 2017-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <cactus_def.h>
#include <cactus_platform_def.h>

	.globl	cactus_entrypoint
	.globl	secondary_cold_entry

/* Provision one stack per Execution Context (or vCPU) */
.section .bss.stacks
	.balign CACHE_WRITEBACK_GRANULE
	.fill	CACTUS_STACKS_SIZE * PLAT_CACTUS_CORE_COUNT
stacks_end:

func cactus_entrypoint
	/* Entry reason is primary EC cold boot */
	mov	x19, #1
secondary_cold_entry:
	/* Saving parameter registers to temporary registers. */
	mov x10, x1
	mov x11, x2
	mov x12, x3
	mov x13, x4

	/* Entry reason is secondary EC cold boot */
	mrs	x0, mpidr_el1
	bl	platform_get_core_pos

	/* Setup the stack pointer. */
	adr	x1, stacks_end
	mov	x2, #CACTUS_STACKS_SIZE
	mul	x2, x0, x2
	sub	sp, x1, x2

	/*
	 * Invalidate the data cache for the whole partition.
	 * This prevents re-use of stale data cache entries from
	 * prior bootloader stages.
	 */
	adrp	x0, __TEXT_START__
	adrp	x1, __BSS_END__
	sub	x1, x1, x0
	bl	inv_dcache_range

	/* Enable I-Cache */
	mrs	x1, sctlr_el1
	orr	x1, x1, #SCTLR_I_BIT
	msr	sctlr_el1, x1
	isb

	/*
	 * Set CPACR_EL1.FPEN=11 no EL1/0 trapping of
	 * SVE/Adv. SIMD/FP instructions.
	 */
	mov	x1, CPACR_EL1_FPEN(CPACR_EL1_FP_TRAP_NONE)
	mrs	x0, cpacr_el1
	orr	x0, x0, x1
	msr	cpacr_el1, x0
	isb

	/* Set up exceptions vector table */
	adrp	x1, cactus_vector
	add	x1, x1, :lo12:cactus_vector
	msr	vbar_el1, x1
	isb

	/* Skip to main if warm boot */
	cbz	x19, 0f

	/* Relocate symbols */
pie_fixup:
	ldr	x0, =pie_fixup
	and	x0, x0, #~(0x1000 - 1)
	mov	x1, #CACTUS_IMAGE_SIZE
	add	x1, x1, x0
	bl	fixup_gdt_reloc

	/* Jump to the C entrypoint (it does not return) */
0:	mov	x0, x19

	/* Restoring parameter registers before jumping to cactus_main. */
	mov x1, x10
	mov x2, x11
	mov x3, x12
	mov x4, x13

	/* And jump to the C entrypoint. */
	b	cactus_main
endfunc cactus_entrypoint
