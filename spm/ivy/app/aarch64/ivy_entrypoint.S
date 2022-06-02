/*
 * Copyright (c) 2018-2022, Arm Limited. All rights reserved.
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

/* Call FFA_MEM_PERM_SET_32 to set the permissions of a given memory region. */
.macro ffa_mem_perm_set start:req end:req perm:req
	adrp x29, \start
	add x29, x29, :lo12:\start

	adrp x30, \end
	add x30, x30, :lo12:\end

	/* x30 = end - start */
	sub x30, x30, x29
	/* x28 = x30 >> 12 (number of pages) */
	mov x28, #12
	lsrv x28, x30, x28

	/* 0x84000089 is function identifier for FFA_MEM_PERM_SET_32 */
	mov w0, #0x89
	movk w0, 0x8400, lsl #16
	mov x1, x29 /* Base VA */
	mov x2, x28 /* Page count */
	mov w3, #\perm /* Memory permissions */
	svc #0

	/* 0x84000061 is function identifier for FFA_SUCCESS_32 */
	mov w1, #0x61
	movk w1, #0x8400, lsl #16
	cmp w1, w0
	b.ne .
.endm

func ivy_entrypoint

	/* Setup the stack pointer. */
	adr	x0, stacks_end
	mov	sp, x0

#if IVY_SHIM == 0
	/* RODATA+DATA+BSS marked RW so relocations can succeed. */
	ffa_mem_perm_set __RODATA_START__ __BSS_END__ 5

	/* Relocate symbols */
ivy_pie_fixup:
	mov	x0, #0x1000
	mov	x1, #IVY_IMAGE_SIZE
	add	x1, x1, x0
	bl	fixup_gdt_reloc

	/* Clear S-EL0 partition BSS */
	adrp	x0, __BSS_START__
	adrp	x2, __BSS_END__
	sub	x2, x2, x0
	mov	x1, xzr
	bl	memset

	/* Then mark RODATA as RO */
	ffa_mem_perm_set __RODATA_START__ __RODATA_END__ 7
#endif /* IVY_SHIM == 0 */

	/* And jump to the C entrypoint. */
	b	ivy_main

endfunc ivy_entrypoint
