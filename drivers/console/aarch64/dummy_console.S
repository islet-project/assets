/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <drivers/console.h>

	.globl	console_init
	.globl	console_putc
	.globl	console_getc
	.globl	console_try_getc
	.globl	console_flush

	/* -----------------------------------------------
	 * int console_init(uintptr_t base_addr,
	 * unsigned int uart_clk, unsigned int baud_rate)
	 *
	 * Clobber list : x1 - x3
	 * -----------------------------------------------
	 */
func console_init
	mov	w0, #1
	ret
endfunc console_init

	/* ---------------------------------------------
	 * int console_putc(int c)
	 *
	 * Clobber list : x1, x2
	 * ---------------------------------------------
	 */
func console_putc
	/* Return the printed characted, which is in x0 */
	ret
endfunc console_putc

	/* ---------------------------------------------
	 * int console_getc(void)
	 *
	 * Clobber list : x0 - x3
	 * ---------------------------------------------
	 */
func console_getc
	mov	w0, #ERROR_NO_VALID_CONSOLE
	ret
endfunc console_getc

	/* ---------------------------------------------
	 * int console_try_getc(void)
	 *
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_try_getc
	mov	w0, #ERROR_NO_PENDING_CHAR
	ret
endfunc console_try_getc

	/* ---------------------------------------------
	 * int console_flush(void)
	 *
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_flush
	mov	w0, #0
	ret
endfunc console_flush
