/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <ivy_def.h>
#include <platform_def.h>

	.globl	ivy_entrypoint

.section .bss.stacks
	.balign CACHE_WRITEBACK_GRANULE
	.fill	IVY_STACKS_SIZE
stacks_end:

func ivy_entrypoint

	/* Setup the stack pointer. */
	adr	x0, stacks_end
	mov	sp, x0
	adr	x0, spm_shim_exceptions_ptr
	msr	vbar_el1, x0

	/* Enable I-Cache */
	mrs	x0, sctlr_el1
	orr	x0, x0, #SCTLR_I_BIT
	msr	sctlr_el1, x0
	isb

	/* Relocate symbols */
pie_fixup:
	ldr	x0, =pie_fixup
	and	x0, x0, #~(0x1000 - 1)
	mov	x1, #IVY_IMAGE_SIZE
	add	x1, x1, x0
	bl	fixup_gdt_reloc


	/* And jump to the C entrypoint. */
	b	ivy_main

endfunc ivy_entrypoint
