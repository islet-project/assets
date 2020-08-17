/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <sdei.h>

	.globl	serror_sdei_event_handler
/*
 * SDEI event handler for SErrors.
 */
func serror_sdei_event_handler
	stp	x29, x30, [sp, #-16]!
	bl	sdei_handler
	ldp	x29, x30, [sp], #16
	mov_imm	x0, SDEI_EVENT_COMPLETE
	mov	x1, xzr
	smc	#0
	b	.
endfunc serror_sdei_event_handler
