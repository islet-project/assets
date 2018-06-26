/*
 * Copyright (c) 2017-2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <secure_partition.h>
#include <spm_svc.h>
#include <xlat_tables_defs.h>

	.globl	cactus_entrypoint

func cactus_entrypoint

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

endfunc cactus_entrypoint
