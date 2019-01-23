/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <platform_def.h>

	.global platform_get_core_pos
	.global plat_crash_console_init
	.global plat_crash_console_flush
	.global plat_crash_console_putc

/*----------------------------------------------------------------------
 * unsigned int platform_get_core_pos(u_register_t mpid)
 *
 * Function to calculate the core position on Hikey960. Since Hikey960
 * doesn't feature multithreaded cores, we ignore the MT bit.
 * clobbers: x0 - x3
 * ---------------------------------------------------------------------
 */
func platform_get_core_pos
	/* x1 = core-id inside cluster */
	ubfx	x1, x0, #MPIDR_AFF0_SHIFT, #MPIDR_AFFINITY_BITS
	/* x2 = cluster-id */
	ubfx	x2, x0, #MPIDR_AFF1_SHIFT, #MPIDR_AFFINITY_BITS

	/* core-position = cluster-id * cores per cluster + core-id */
	mov	x3, #PLATFORM_CORE_COUNT_PER_CLUSTER
	madd	x0, x2, x3, x1
	ret
endfunc platform_get_core_pos

	/* ---------------------------------------------
	 * int plat_crash_console_init(void)
	 * Function to initialize the crash console
	 * without a C Runtime to print crash report.
	 * Clobber list : x0 - x4
	 * ---------------------------------------------
	 */
func plat_crash_console_init
	mov_imm	x0, CRASH_CONSOLE_BASE
	mov_imm	x1, PL011_UART_CLK_IN_HZ
	mov_imm	x2, PL011_BAUDRATE
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
	mov_imm	x1, CRASH_CONSOLE_BASE
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
	mov_imm	x1, CRASH_CONSOLE_BASE
	b	console_core_flush
endfunc plat_crash_console_flush
