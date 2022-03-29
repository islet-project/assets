/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.macro service_call _conduit
	/* Push r9 to keep the stack pointer aligned to 64 bit. */
	push	{r4 - r9}

	/*
	 * Save the ffa_value pointer in a callee saved register.
	 */
	mov	r8, r0

	/* Load the argument values into the appropriate registers. */
	ldm	r0, {r0 - r7}

	\_conduit	#0

	/*
	 * The return values are stored in x0-x7, put them in the ffa_value
	 * return structure.
	 */
	stm	r8, {r0 - r7}

	pop	{r4 - r9}
	.endm

.globl ffa_svc
func ffa_svc
	service_call svc
	bx	lr
endfunc ffa_svc

.globl ffa_smc
func ffa_smc
	service_call smc
	bx	lr
endfunc ffa_smc
