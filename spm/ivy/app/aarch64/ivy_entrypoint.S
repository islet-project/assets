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

	/* And jump to the C entrypoint. */
	b	ivy_main

endfunc ivy_entrypoint
