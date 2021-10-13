/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
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
	 * Enable the instruction cache and alignment checks.
	 * --------------------------------------------------------------------
	 */
	mov	x1, #(SCTLR_I_BIT | SCTLR_A_BIT | SCTLR_SA_BIT)
	asm_read_sctlr_el1_or_el2
	orr	x0, x0, x1
	asm_write_sctlr_el1_or_el2 x1
	isb

	/* --------------------------------------------------------------------
	 * Invalidate the cache in the RW memory range to safeguard
	 * against possible stale data in the cache from previous
	 * firmware stage.
	 * --------------------------------------------------------------------
	 */
	ldr	x0, =__RW_START__
	ldr	x1, =__RW_END__
	sub	x1, x1, x0
	bl	inv_dcache_range

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
	 * ---------------------------------------------------------------------
	 */
	mrs	x0, mpidr_el1
	bl	platform_set_stack

	/* ---------------------------------------------------------------------
	 * Perform early platform setup & platforms specific early architectural
	 * setup, e.g. MMU setup.
	 * ---------------------------------------------------------------------
	 */
	bl	tftf_early_platform_setup
	bl	tftf_plat_arch_setup

#if ENABLE_PAUTH
	/* ---------------------------------------------------------------------
	 * Program APIAKey_EL1 and enable pointer authentication.
	 * ---------------------------------------------------------------------
	 */
	bl	pauth_init_enable
#endif /* ENABLE_PAUTH */

	/* ---------------------------------------------------------------------
	 * Jump to main function.
	 * ---------------------------------------------------------------------
	 */
	bl	ns_bl2u_main
dead:
	b	dead
endfunc ns_bl2u_entrypoint
