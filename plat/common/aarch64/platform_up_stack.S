/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <platform_def.h>


	.local	platform_normal_stacks
	.globl	platform_set_stack
	.globl	platform_get_stack

	/* -----------------------------------------------------
	 * unsigned long platform_get_stack (unsigned long)
	 *
	 * For cold-boot images, only the primary CPU needs a
	 * stack. This function returns the stack pointer for a
	 * stack allocated in device memory.
	 * -----------------------------------------------------
	 */
func platform_get_stack
	get_up_stack platform_normal_stacks, PLATFORM_STACK_SIZE
	ret
endfunc platform_get_stack

	/* -----------------------------------------------------
	 * void platform_set_stack (unsigned long)
	 *
	 * For cold-boot images, only the primary CPU needs a
	 * stack. This function sets the stack pointer to a stack
	 * allocated in normal memory.
	 * -----------------------------------------------------
	 */
func platform_set_stack
	get_up_stack platform_normal_stacks, PLATFORM_STACK_SIZE
	mov sp, x0
	ret
endfunc platform_set_stack

	/* -----------------------------------------------------
	 * Single cpu stack in normal memory.
	 * Used for C code during boot, PLATFORM_STACK_SIZE bytes
	 * are allocated
	 * -----------------------------------------------------
	 */
declare_stack platform_normal_stacks, ns_bl_normal_stacks, \
		PLATFORM_STACK_SIZE, 1
