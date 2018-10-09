/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <platform_def.h>


	.local	pcpu_dv_mem_stack
	.local	platform_normal_stacks
	.weak	platform_set_stack
	.weak	platform_get_stack
	.weak	platform_set_coherent_stack


	/* -----------------------------------------------------
	 * void platform_set_coherent_stack (unsigned long mpidr)
	 *
	 * For a given CPU, this function sets the stack pointer
	 * to a stack allocated in device memory. This stack can
	 * be used by C code which enables/disables the SCTLR.M
	 * SCTLR.C bit e.g. while powering down a cpu
	 * -----------------------------------------------------
	 */
func platform_set_coherent_stack
	mov x9, x30 // lr
	get_mp_stack pcpu_dv_mem_stack, PCPU_DV_MEM_STACK_SIZE
	mov sp, x0
	ret x9
endfunc platform_set_coherent_stack

	/* -----------------------------------------------------
	 * unsigned long platform_get_stack (unsigned long mpidr)
	 *
	 * For a given CPU, this function returns the stack
	 * pointer for a stack allocated in device memory.
	 * -----------------------------------------------------
	 */
func platform_get_stack
	mov x10, x30 // lr
	get_mp_stack platform_normal_stacks, PLATFORM_STACK_SIZE
	ret x10
endfunc platform_get_stack

	/* -----------------------------------------------------
	 * void platform_set_stack (unsigned long mpidr)
	 *
	 * For a given CPU, this function sets the stack pointer
	 * to a stack allocated in normal memory.
	 * -----------------------------------------------------
	 */
func platform_set_stack
	mov x9, x30 // lr
	bl  platform_get_stack
	mov sp, x0
	ret x9
endfunc platform_set_stack

	/* -----------------------------------------------------
	 * Per-cpu stacks in normal memory.
	 * Used for C code during runtime execution (when coherent
	 * stacks are not required).
	 * Each cpu gets a stack of PLATFORM_STACK_SIZE bytes.
	 * -----------------------------------------------------
	 */
declare_stack platform_normal_stacks, tftf_normal_stacks, \
		PLATFORM_STACK_SIZE, PLATFORM_CORE_COUNT

	/* -----------------------------------------------------
	 * Per-cpu stacks in device memory.
	 * Used for C code just before power down or right after
	 * power up when the MMU or caches need to be turned on
	 * or off.
	 * Each cpu gets a stack of PCPU_DV_MEM_STACK_SIZE bytes.
	 * -----------------------------------------------------
	 */
declare_stack pcpu_dv_mem_stack, tftf_coherent_stacks, \
		PCPU_DV_MEM_STACK_SIZE, PLATFORM_CORE_COUNT
