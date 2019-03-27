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
	 * Function to initialize the console without a
	 * C Runtime to print debug information. It saves
	 * the console base to the data section.
	 * In: x0 - Console base address
	 *     w1 - Uart clock in Hz
	 *     w2 - Baud rate
	 * Out: w0 - Return 1 on success, 0 on error.
	 * Clobber list : x1 - x3
	 * -----------------------------------------------
	 */
func console_init
	mov	w0, #1
	ret
endfunc console_init

	/* ---------------------------------------------
	 * int console_putc(int c)
	 * Function to output a character over the
	 * console. It returns the character printed on
	 * success or an error code.
	 * In : x0 - Character to be printed
	 * Out : w0 - Input character or error code.
	 * Clobber list : x1, x2
	 * ---------------------------------------------
	 */
func console_putc
	/* Return the printed characted, which is in x0 */
	ret
endfunc console_putc

	/* ---------------------------------------------
	 * int console_getc(void)
	 * Function to get a character from the console.
	 * It returns the character grabbed on success
	 * or an error code on error. This function is
	 * blocking, it waits until there is an
	 * available character to return.
	 * Out : w0 - Return character or error code.
	 * Clobber list : x0 - x3
	 * ---------------------------------------------
	 */
func console_getc
	mov	w0, #ERROR_NO_VALID_CONSOLE
	ret
endfunc console_getc

	/* ---------------------------------------------
	 * int console_try_getc(void)
	 * Function to get a character from the console.
	 * It returns the character grabbed on success
	 * or an error code on error. This function is
	 * non-blocking, it returns immediately.
	 * Out : w0 - Return character or error code.
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_try_getc
	mov	w0, #ERROR_NO_PENDING_CHAR
	ret
endfunc console_try_getc

	/* ---------------------------------------------
	 * int console_flush(void)
	 * Function to force a write of all buffered
	 * data that hasn't been output. It returns 0
	 * upon successful completion, otherwise it
	 * returns an error code.
	 * Out: w0 - Error code or 0.
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_flush
	mov	w0, #0
	ret
endfunc console_flush
