/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <drivers/arm/pl011.h>
#include <drivers/console.h>

	.globl	console_init
	.globl	console_putc
	.globl	console_getc
	.globl	console_try_getc
	.globl	console_flush
	.globl	console_core_init
	.globl	console_core_putc
	.globl	console_core_getc
	.globl	console_core_flush

	/*
	 *  The console base is in the data section and not in .bss
	 *  even though it is zero-init. In particular, this allows
	 *  the console functions to start using this variable before
	 *  the runtime memory is initialized for images which do not
	 *  need to copy the .data section from ROM to RAM.
	 */
	.section .data.console_base
	.align 3
console_base: .quad 0x0

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
	adrp	x3, console_base
	str	x0, [x3, :lo12:console_base]
	b	console_core_init
endfunc console_init

	/* -----------------------------------------------
	 * int console_core_init(uintptr_t base_addr,
	 * unsigned int uart_clk, unsigned int baud_rate)
	 * Function to initialize the console without a
	 * C Runtime to print debug information. This
	 * function will be accessed by console_init and
	 * crash reporting.
	 * In: x0 - Console base address
	 *     w1 - Uart clock in Hz
	 *     w2 - Baud rate
	 * Out: w0 - Return 1 on success, 0 on error.
	 * Clobber list : x1 - x3
	 * -----------------------------------------------
	 */
func console_core_init
	/* Check the input base address */
	cbz	x0, init_fail
	/* Check baud rate and uart clock for sanity */
	cbz	w1, init_fail
	cbz	w2, init_fail
	/* Disable uart before programming */
	ldr	w3, [x0, #UARTCR]
	bic	w3, w3, #PL011_UARTCR_UARTEN
	str	w3, [x0, #UARTCR]
	/* Program the baudrate */
	/* Divisor =  (Uart clock * 4) / baudrate */
	lsl	w1, w1, #2
	udiv	w2, w1, w2
	/* IBRD = Divisor >> 6 */
	lsr	w1, w2, #6
	/* Write the IBRD */
	str	w1, [x0, #UARTIBRD]
	/* FBRD = Divisor & 0x3F */
	and	w1, w2, #0x3f
	/* Write the FBRD */
	str	w1, [x0, #UARTFBRD]
	mov	w1, #PL011_LINE_CONTROL
	str	w1, [x0, #UARTLCR_H]
	/* Clear any pending errors */
	str	wzr, [x0, #UARTECR]
	/* Enable tx, rx, and uart overall */
	mov	w1, #(PL011_UARTCR_RXE | PL011_UARTCR_TXE | PL011_UARTCR_UARTEN)
	str	w1, [x0, #UARTCR]
	mov	w0, #1
	ret
init_fail:
	mov	w0, wzr
	ret
endfunc console_core_init

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
	adrp	x1, console_base
	ldr	x1, [x1, :lo12:console_base]
	b	console_core_putc
endfunc console_putc

	/* ---------------------------------------------
	 * int console_core_putc(int c, uintptr_t base_addr)
	 * Function to output a character over the console. It
	 * returns the character printed on success or an error
	 * code.
	 * In : w0 - Character to be printed
	 *      x1 - Console base address
	 * Out : w0 - Input character or error code.
	 * Clobber list : x2
	 * ---------------------------------------------
	 */
func console_core_putc
	/* Check the input parameter */
	cbz	x1, putc_error
	/* Prepend '\r' to '\n' */
	cmp	w0, #0xA
	b.ne	2f
1:
	/* Check if the transmit FIFO is full */
	ldr	w2, [x1, #UARTFR]
	tbnz	w2, #PL011_UARTFR_TXFF_BIT, 1b
	mov	w2, #0xD
	str	w2, [x1, #UARTDR]
2:
	/* Check if the transmit FIFO is full */
	ldr	w2, [x1, #UARTFR]
	tbnz	w2, #PL011_UARTFR_TXFF_BIT, 2b

	/* Only write 8 bits */
	and	w0, w0, #0xFF
	str	w0, [x1, #UARTDR]
	ret
putc_error:
	mov	w0, #ERROR_NO_VALID_CONSOLE
	ret
endfunc console_core_putc

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
	adrp	x2, console_base
	ldr	x2, [x2, :lo12:console_base]
	mov	x3, x30

	/* Loop until it returns a character or an error. */
1:	mov	x0, x2
	bl	console_core_getc
	cmp	w0, #ERROR_NO_PENDING_CHAR
	b.eq	1b

	ret	x3
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
	adrp	x0, console_base
	ldr	x0, [x0, :lo12:console_base]
	b	console_core_getc
endfunc console_try_getc

	/* ---------------------------------------------
	 * int console_core_getc(uintptr_t base_addr)
	 * Function to get a character from the console.
	 * It returns the character grabbed on success
	 * or an error code.
	 * In : x0 - Console base address
	 * Out : w0 - Return character or error code.
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_core_getc
	cbz	x0, getc_error

	/* Check if the receive FIFO is empty */
	ldr	w1, [x0, #UARTFR]
	tbnz	w1, #PL011_UARTFR_RXFE_BIT, getc_empty

	/* Read a character from the FIFO */
	ldr	w0, [x0, #UARTDR]
	/* Mask out error flags */
	and	w0, w0, #0xFF
	ret

getc_empty:
	mov	w0, #ERROR_NO_PENDING_CHAR
	ret
getc_error:
	mov	w0, #ERROR_NO_VALID_CONSOLE
	ret
endfunc console_core_getc

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
	adrp	x0, console_base
	ldr	x0, [x0, :lo12:console_base]
	b	console_core_flush
endfunc console_flush

	/* ---------------------------------------------
	 * int console_core_flush(uintptr_t base_addr)
	 * Function to force a write of all buffered
	 * data that hasn't been output.
	 * In : x0 - Console base address
	 * Out : w0 - Error code or 0.
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_core_flush
	cbz	x0, flush_error

1:
	/* Loop until the transmit FIFO is empty */
	ldr	w1, [x0, #UARTFR]
	tbnz	w1, #PL011_UARTFR_BUSY_BIT, 1b

	mov	w0, wzr
	ret
flush_error:
	mov	w0, #ERROR_NO_VALID_CONSOLE
	ret
endfunc console_core_flush
