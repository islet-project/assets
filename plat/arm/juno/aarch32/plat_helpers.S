/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <drivers/arm/pl011.h>
#include "../juno_def.h"

	.globl	platform_get_core_pos
	.globl	plat_crash_console_init
	.globl	plat_crash_console_putc
	.globl	plat_crash_console_flush

	/*
	 * Return 0 to 3 for the Cortex-A53 cores and 4 to 5 for the Cortex-A57
	 * cores.
	 */
func platform_get_core_pos
	and	r1, r0, #MPIDR_CPU_MASK
	and	r0, r0, #MPIDR_CLUSTER_MASK
	/* Swap Cortex-A53/Cortex-A57 order. */
	eor	r0, r0, #(1 << MPIDR_AFF1_SHIFT)
	add	r0, r1, r0, LSR #6
	bx	lr
endfunc platform_get_core_pos

	/* ---------------------------------------------
	 * int plat_crash_console_init(void)
	 * Function to initialize the crash console
	 * without a C Runtime to print crash report.
	 * Clobber list : x0 - x4
	 * ---------------------------------------------
	 */
func plat_crash_console_init
	ldr	r0, =PLAT_ARM_UART_BASE
	ldr	r1, =PLAT_ARM_UART_CLK_IN_HZ
	ldr	r2, =PL011_BAUDRATE
	b	console_core_init
endfunc plat_crash_console_init

	/* ---------------------------------------------
	 * int plat_crash_console_putc(int c)
	 * Function to print a character on the crash
	 * console without a C Runtime.
	 * Clobber list : x1, x2
	 * ---------------------------------------------
	 */
func plat_crash_console_putc
	ldr	r1, =PLAT_ARM_UART_BASE
	b	console_core_putc
endfunc plat_crash_console_putc

	/* ---------------------------------------------
	 * int plat_crash_console_flush()
	 * Function to force a write of all buffered
	 * data that hasn't been output.
	 * Out : return -1 on error else return 0.
	 * Clobber list : r0 - r1
	 * ---------------------------------------------
	 */
func plat_crash_console_flush
	ldr	r1, =PLAT_ARM_UART_BASE
	b	console_core_flush
endfunc plat_crash_console_flush
