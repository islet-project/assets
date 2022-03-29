/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.macro service_call _conduit
	/*
	 * Save the address of the ffa_value structure on the stack.
	 *
	 * Although x0 contains an 8-byte value, we are allocating 16 bytes
	 * on the stack to respect the 16-byte stack-alignment.
	 */
	str	x0, [sp, #-16]!

	/* Load the argument values into the appropriate registers. */
	ldp	x6, x7, [x0, #48]
	ldp	x4, x5, [x0, #32]
	ldp	x2, x3, [x0, #16]
	ldp	x0, x1, [x0, #0]

	\_conduit	#0

	/*
	 * Pop the ffa_value structure address from the stack into a
	 * caller-saved register.
	 */
	ldr	x9, [sp], #16

	/*
	 * The return values are stored in x0-x7, put them in the ffa_value
	 * return structure.
	 */
	stp	x0, x1, [x9, #0]
	stp	x2, x3, [x9, #16]
	stp	x4, x5, [x9, #32]
	stp	x6, x7, [x9, #48]

	.endm

.globl ffa_svc
func ffa_svc
	service_call svc
	ret
endfunc ffa_svc

.globl ffa_smc
func ffa_smc
	service_call smc
	ret
endfunc ffa_smc
