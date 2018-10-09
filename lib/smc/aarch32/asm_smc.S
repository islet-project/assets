/*
 * Copyright (c) 2016-2017, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.globl	asm_tftf_smc32

/* ---------------------------------------------------------------------------
 * void asm_tftf_smc32(const smc_args *args,
 *                                smc_ret_values *smc_ret);
 * ---------------------------------------------------------------------------
 */
func asm_tftf_smc32
	/* Push r9 to keep the stack pointer aligned to 64 bit. */
	push	{r4 - r9}

	/* Store the `smc_ret` pointer in a callee saved register */
	mov	r8, r1

	/* Load values used as arguments for the SMC. */
	ldm	r0, {r0 - r7}

	smc	#0

	/*
	 * The returned values from the SMC are in r0-r3, put them in the
	 * 'smc_ret_values' return structure.
	 */
	stm	r8, {r0 - r3}

	pop	{r4 - r9}
	bx	lr
endfunc asm_tftf_smc32
