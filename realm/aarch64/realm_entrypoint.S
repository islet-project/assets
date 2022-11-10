/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <realm_def.h>

	.globl	realm_entrypoint

.section .bss.stacks
	.fill	REALM_STACK_SIZE
stacks_end:

func realm_entrypoint
	/* Setup the stack pointer. */
	adr	x1, stacks_end
	mov	sp, x1

	/* Clear BSS */
	ldr	x0, =__REALM_BSS_START__
	adr	x1, realm_entrypoint
	add	x0, x1, x0
	ldr	x1, =__REALM_BSS_SIZE__
	bl	zeromem16

	/*
	 * Invalidate the data cache for the whole Realm.
	 * This prevents re-use of stale data cache entries from
	 * prior bootloader stages.
	 */
	adrp	x0, __REALM_TEXT_START__
	add	x0, x0, realm_entrypoint
	adrp	x1, __REALM_BSS_END__
	add	x1, x1, realm_entrypoint
	sub	x1, x1, x0
	bl	inv_dcache_range

	/* Initialize architectural state. */
	bl	arch_init

	/* Relocate symbols */
pie_fixup:
	ldr	x0, =pie_fixup
	and	x0, x0, #~(PAGE_ALIGNMENT - 1)
	mov	x1, REALM_MAX_LOAD_IMG_SIZE
	add	x1, x1, x0
	bl	fixup_gdt_reloc

	/* And jump to the C entrypoint. */
	b	realm_payload_main
endfunc realm_entrypoint

/* Initialize architectural state. */
func arch_init
	/* Set the exception vectors. */
	adr	x0, realm_vector
	add	x1, x1, :lo12:realm_vector
	msr	vbar_el1, x0
	isb

	/* Enable the instruction cache and stack pointer alignment checks. */
	mov_imm	x0, (SCTLR_EL1_RES1 | SCTLR_I_BIT | SCTLR_A_BIT | SCTLR_SA_BIT)
	msr	sctlr_el1, x0

	/*
	 * Set CPACR_EL1.FPEN=11 no EL1/0 trapping of
	 * SVE/Adv. SIMD/FP instructions.
	 */
	mov	x1, CPACR_EL1_FPEN(CPACR_EL1_FP_TRAP_NONE)
	mrs	x0, cpacr_el1
	orr	x0, x0, x1
	msr	cpacr_el1, x0
	isb

	ret
endfunc arch_init
