/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <tftf.h>

	.globl	ns_bl1u_entrypoint

func ns_bl1u_entrypoint
	/* --------------------------------------------------------------------
	 * Set the exception vectors.
	 * --------------------------------------------------------------------
	 */
	ldr	r0, =exception_stubs
	stcopr	r0, HVBAR

	/* --------------------------------------------------------------------
	 * Enable the instruction cache.
	 * --------------------------------------------------------------------
	 */
	ldcopr	r0, HSCTLR
	ldr	r1, =HSCTLR_I_BIT
	orr	r0, r0, r1
	stcopr	r0, HSCTLR
	isb

	/* ---------------------------------------------------------------------
	 * Init C runtime environment.
	 *   - Zero-initialise the .bss section;
	 *   - Copy the data section from NS-BL1U image (stored in ROM) to the
	 *     correct location in RAM.
	 * ---------------------------------------------------------------------
	 */
	ldr	r0, =__BSS_START__
	ldr	r1, =__BSS_SIZE__
	bl	zeromem

	ldr	r0, =__DATA_RAM_START__
	ldr	r1, =__DATA_ROM_START__
	ldr	r2, =__DATA_SIZE__
	bl	memcpy4

	/* ---------------------------------------------------------------------
	 * Allocate a stack whose memory will be marked as Normal
	 * Inner-Shareable, Write-Back, Write-Allocate memory when the MMU is
	 * enabled.
	 * There is no risk of reading stale stack memory after enabling the MMU
	 * as only the primary CPU is running at the moment.
	 * ---------------------------------------------------------------------
	 */
	ldcopr	r0, MPIDR
	bl	platform_set_stack

	/* ---------------------------------------------------------------------
	 * Perform early platform setup & platform specific early architectural
	 * setup, e.g. MMU setup.
	 * ---------------------------------------------------------------------
	 */
	bl	tftf_early_platform_setup
	bl	tftf_plat_arch_setup

	/* ---------------------------------------------------------------------
	 * Jump to main function.
	 * ---------------------------------------------------------------------
	 */
	bl	ns_bl1u_main
dead:
	b	dead
endfunc ns_bl1u_entrypoint
