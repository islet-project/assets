/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>

	.global	pauth_init_enable

/* -----------------------------------------------------------
 * Program APIAKey_EL1 key and enable Pointer Authentication
 * of instruction addresses in the current translation regime
 * for the calling CPU.
 * -----------------------------------------------------------
 */
func pauth_init_enable
	stp	x29, x30, [sp, #-16]!

	/* Initialize platform key */
	bl	init_apkey

	/*
	 * Program instruction key A used by
	 * the Trusted Firmware Test Framework
	 */
	msr	APIAKeyLo_EL1, x0
	msr	APIAKeyHi_EL1, x1

	/* Detect Current Exception level */
	mrs	x0, CurrentEL
	cmp	x0, #(MODE_EL1 << MODE_EL_SHIFT)
	b.eq	enable_el1

	/* Enable EL2 pointer authentication */
	mrs	x0, sctlr_el2
	orr	x0, x0, #SCTLR_EnIA_BIT
	msr	sctlr_el2, x0
	b	enable_exit

	/* Enable EL1 pointer authentication */
enable_el1:
	mrs	x0, sctlr_el1
	orr	x0, x0, #SCTLR_EnIA_BIT
	msr	sctlr_el1, x0

enable_exit:
	isb

	ldp	x29, x30, [sp], #16
	ret
endfunc pauth_init_enable
