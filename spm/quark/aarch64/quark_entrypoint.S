/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <quark_def.h>
#include <platform_def.h>

	.globl	quark_entrypoint

.section .bss.stacks
	.balign CACHE_WRITEBACK_GRANULE
	.fill	QUARK_STACKS_SIZE
stacks_end:

func quark_entrypoint

	/* Setup the stack pointer. */
	adr	x0, stacks_end
	mov	sp, x0

	/* And jump to the C entrypoint. */
	b	quark_main

endfunc quark_entrypoint
