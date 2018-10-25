/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <tftf.h>

	.globl	ns_bl2u_entrypoint

func ns_bl2u_entrypoint
	/* ---------------------------------------------------------------------
	 * Set the exception vectors.
	 * ---------------------------------------------------------------------
	 */
	adr	x0, exception_stubs
	asm_write_vbar_el1_or_el2 x1

	/* --------------------------------------------------------------------
	 * Enable the instruction cache and stack pointer alignment checks.
	 * --------------------------------------------------------------------
	 */
	mov	x1, #(SCTLR_I_BIT | SCTLR_SA_BIT)
	asm_read_sctlr_el1_or_el2
	orr	x0, x0, x1
	asm_write_sctlr_el1_or_el2 x1
	isb

	/* ---------------------------------------------------------------------
	 * Zero-initialise the .bss section.
	 * ---------------------------------------------------------------------
	 */
	ldr	x0, =__BSS_START__
	ldr	x1, =__BSS_SIZE__
	bl	zeromem16

	/* ---------------------------------------------------------------------
	 * Allocate a stack whose memory will be marked as Normal
	 * Inner-Shareable, Write-Back, Write-Allocate memory when the MMU is
	 * enabled.
	 * There is no risk of reading stale stack memory after enabling the MMU
	 * as only the primary CPU is running at the moment.
	 * ---------------------------------------------------------------------
	 */
	mrs	x0, mpidr_el1
	bl	platform_set_stack

	/* ---------------------------------------------------------------------
	* Perform early platform setup & platforms specific early architectural
	* setup, e.g. MMU setup.
	* ----------------------------------------------------------------------
	*/
	bl	tftf_early_platform_setup
	bl	tftf_plat_arch_setup

	/* ---------------------------------------------------------------------
	 * Jump to main function.
	 * ---------------------------------------------------------------------
	 */
	bl	ns_bl2u_main
dead:
	b	dead
endfunc ns_bl2u_entrypoint
