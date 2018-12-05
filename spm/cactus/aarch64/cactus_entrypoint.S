/*
 * Copyright (c) 2017-2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <cactus_def.h>
#include <platform_def.h>

	.globl	cactus_entrypoint

.section .bss.stacks
	.balign CACHE_WRITEBACK_GRANULE
	.fill	CACTUS_STACKS_SIZE
stacks_end:

func cactus_entrypoint

	/* Save the base address and size of the buffer. */
	mov	x20, x0
	mov	x21, x1

	/* Setup the stack pointer. */
	adr	x0, stacks_end
	mov	sp, x0

	/* And do the rest in C code */
	mov	x0, x20
	mov	x1, x21
	b	cactus_main

endfunc cactus_entrypoint
