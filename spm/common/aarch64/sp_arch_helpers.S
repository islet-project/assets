/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.globl	sp_svc

func sp_svc
	/*
	 * Save the address of the svc_args structure on the stack.
	 *
	 * Although x0 contains an 8-byte value, we are allocating 16 bytes
	 * on the stack to respect the 16-byte stack-alignment.
	 */
	str	x0, [sp, #-16]!

	/* Load the SVC arguments values into the appropriate registers. */
	ldp	x6, x7, [x0, #48]
	ldp	x4, x5, [x0, #32]
	ldp	x2, x3, [x0, #16]
	ldp	x0, x1, [x0, #0]

	svc	#0

	/*
	 * Pop the svc_args structure address from the stack into a caller-saved
	 * register.
	 */
	ldr	x9, [sp], #16

	/*
	 * The return values are stored in x0-x3, put them in the svc_args
	 * return structure.
	 */
	stp	x0, x1, [x9, #0]
	stp	x2, x3, [x9, #16]
	ret
endfunc sp_svc
