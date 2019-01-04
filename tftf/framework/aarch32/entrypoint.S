/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <tftf.h>

	.globl	tftf_entrypoint
	.globl	tftf_hotplug_entry

/* ----------------------------------------------------------------------------
 * Cold boot entry point for the primary CPU.
 * ----------------------------------------------------------------------------
 */
func tftf_entrypoint
	bl	arch_init

	/* --------------------------------------------------------------------
	 * This code is expected to be executed only by the primary CPU.
	 * Save the mpid for the first core that executes and if a secondary
	 * CPU has lost its way make it spin forever.
	 * --------------------------------------------------------------------
	 */
	bl	save_primary_mpid

	/* --------------------------------------------------------------------
	 * Zero out NOBITS sections. There are 2 of them:
	 *   - the .bss section;
	 *   - the coherent memory section.
	 * --------------------------------------------------------------------
	 */
	ldr	r0, =__BSS_START__
	ldr	r1, =__BSS_SIZE__
	bl	zeromem

	ldr	r0, =__COHERENT_RAM_START__
	ldr	r1, =__COHERENT_RAM_UNALIGNED_SIZE__
	bl	zeromem

	/* --------------------------------------------------------------------
	 * Give ourselves a small coherent stack to ease the pain of
	 * initializing the MMU
	 * --------------------------------------------------------------------
	 */
	ldcopr	r0, MPIDR
	bl	platform_set_coherent_stack

	bl	tftf_early_platform_setup
	bl	tftf_plat_arch_setup

	/* --------------------------------------------------------------------
	 * Give ourselves a stack allocated in Normal -IS-WBWA memory
	 * --------------------------------------------------------------------
	 */
	ldcopr	r0, MPIDR
	bl	platform_set_stack

	/* --------------------------------------------------------------------
	 * tftf_cold_boot_main() will perform the remaining architectural and
	 * platform setup, initialise the test framework's state, then run the
	 * tests.
	 * --------------------------------------------------------------------
	 */
	b	tftf_cold_boot_main
endfunc tftf_entrypoint

/* ----------------------------------------------------------------------------
 * Entry point for a CPU that has just been powered up.
 * In : r0 - context_id
 * ----------------------------------------------------------------------------
 */
func tftf_hotplug_entry

	/* --------------------------------------------------------------------
	 * Preserve the context_id in a callee-saved register
	 * --------------------------------------------------------------------
	 */
	mov	r4, r0

	bl	arch_init

	/* --------------------------------------------------------------------
	 * Give ourselves a small coherent stack to ease the pain of
	 * initializing the MMU
	 * --------------------------------------------------------------------
	 */
	ldcopr	r0, MPIDR
	bl	platform_set_coherent_stack

	/* --------------------------------------------------------------------
	 * Enable the MMU
	 * --------------------------------------------------------------------
	 */
	bl	tftf_plat_enable_mmu

	/* --------------------------------------------------------------------
	 * Give ourselves a stack in normal memory.
	 * --------------------------------------------------------------------
	 */
	ldcopr	r0, MPIDR
	bl	platform_set_stack

	/* --------------------------------------------------------------------
	 * Save the context_id for later retrieval by tests
	 * --------------------------------------------------------------------
	 */
	ldcopr	r0, MPIDR
	ldr	r1, =MPID_MASK
	and	r0, r0, r1
	bl	platform_get_core_pos

	mov	r1, r4

	bl	tftf_set_cpu_on_ctx_id

	/* --------------------------------------------------------------------
	 * Jump to warm boot main function
	 * --------------------------------------------------------------------
	 */
	b	tftf_warm_boot_main
endfunc tftf_hotplug_entry

/* ----------------------------------------------------------------------------
 * Initialize architectural state.
 * ----------------------------------------------------------------------------
 */
func arch_init
	/* Set the exception vectors. */
	ldr	r0, =tftf_vector
	stcopr	r0, HVBAR

	/* Enable the instruction cache. */
	ldr     r0, =(HSCTLR_RES1 | HSCTLR_I_BIT)
	stcopr	r0, HSCTLR

	isb
	bx	lr
endfunc arch_init

/* ----------------------------------------------------------------------------
 * Saves the mpid of the primary core and if the primary core
 * is already saved then it loops infinitely.
 * ----------------------------------------------------------------------------
 */
func save_primary_mpid
	ldr	r1, =tftf_primary_core
	ldr	r0, [r1]
	mov	r2, #INVALID_MPID
	cmp	r0, r2
	bne	panic
	ldr	r2, =MPID_MASK
	ldcopr	r0, MPIDR
	and	r0, r0, r2
	str	r0, [r1]
	bx	lr
panic:
	/* Primary core MPID already saved */
	b	panic
endfunc save_primary_mpid
