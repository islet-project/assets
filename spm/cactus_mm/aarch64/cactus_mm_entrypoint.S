/*
 * Copyright (c) 2017-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <secure_partition.h>
#include <spm_svc.h>
#include <xlat_tables_defs.h>

	.globl	cactus_mm_entrypoint

func cactus_mm_entrypoint

	/*
	 * All the information needed to remap the memory of the Secure
	 * Partition is in the buffer whose pointer is passed on X0 and size on
	 * X1. If the size is 0, return with an error.
	 */
	cmp	x1, #0
	beq	.return_error

	/* Save the base address and size of the buffer. */
	mov	x20, x0
	mov	x21, x1
	/* Size of the Secure Partition image. */
	ldr	x22, [x20, SP_BOOT_INFO_IMAGE_SIZE_OFFSET]

	/*
	 * Remap all sections of the image before doing anything else.
	 *
	 * Not even the console can be initialized before because it needs to
	 * initialize variables (that can only be modified after remapping that
	 * region as RW).
	 *
	 * If any of the calls fails, loop, as there is no console to print an
	 * error message to.
	 */
	.macro	set_sp_mem_attributes
	cmp	x2, #0 /* If size is 0, skip the call. */
	beq	1f
	mov_imm	x0, SP_MEMORY_ATTRIBUTES_SET_AARCH64
	svc	#0
	cmp	x0, #0
	bne	.return_error
1:
	.endm

	adr	x1, __TEXT_START__
	adr	x2, __TEXT_END__
	sub	x2, x2, x1 /* __TEXT_SIZE__ */
	lsr	x2, x2, PAGE_SIZE_SHIFT /* __TEXT_SIZE__ in pages */
	mov	x3, SP_MEMORY_ATTRIBUTES_ACCESS_RO | SP_MEMORY_ATTRIBUTES_EXEC
	set_sp_mem_attributes

	adr	x1, __RODATA_START__
	adr	x2, __RODATA_END__
	sub	x2, x2, x1 /* __RODATA_SIZE__ */
	lsr	x2, x2, PAGE_SIZE_SHIFT /* __RODATA_SIZE__ in pages */
	mov	x3, SP_MEMORY_ATTRIBUTES_ACCESS_RO | SP_MEMORY_ATTRIBUTES_NON_EXEC
	set_sp_mem_attributes

	adr	x1, __RWDATA_START__
	adr	x2, __RWDATA_END__
	sub	x2, x2, x1 /* __RWDATA_SIZE__ */
	lsr	x2, x2, PAGE_SIZE_SHIFT /* __RWDATA_SIZE__ in pages */
	mov	x3, SP_MEMORY_ATTRIBUTES_ACCESS_RW | SP_MEMORY_ATTRIBUTES_NON_EXEC
	set_sp_mem_attributes

	/*
	 * To avoid accessing it by mistake, prevent EL0 from accessing the rest
	 * of the memory reserved for the Secure Partition.
	 *
	 * Unused size = Total size - Used size
	 *             = Total size - (__RWDATA_END__ -  __TEXT_START__)
	 */
	adr	x1, __RWDATA_END__
	adr	x2, __TEXT_START__
	sub	x2, x1, x2 /* x2 = Used size, x22 = Total size */
	sub	x2, x22, x2 /* x2 = Unused size */
	lsr	x2, x2, PAGE_SIZE_SHIFT /* Unused size in pages */
	mov	x3, SP_MEMORY_ATTRIBUTES_ACCESS_NOACCESS | SP_MEMORY_ATTRIBUTES_NON_EXEC
	set_sp_mem_attributes

	adr	x0, __BSS_START__
	adr	x1, __BSS_END__
	sub	x1, x1, x0
	bl	zeromem16

	/* Setup the stack pointer. */
	ldr	x0, [x20, SP_BOOT_INFO_STACK_BASE_OFFSET]
	ldr	x1, [x20, SP_BOOT_INFO_PCPU_STACK_SIZE_OFFSET]
	add	x0, x0, x1
	mov	sp, x0

	/* And do the rest in C code */
	mov	x0, x20
	mov	x1, x21
	b	cactus_main

.return_error:
	/* Tell SPM that the initialization failed. */
	mov_imm	x0, SP_EVENT_COMPLETE_AARCH64
	mov	x1, #-1
	svc	#0

	/* Loop forever */
	b	.

endfunc cactus_mm_entrypoint
