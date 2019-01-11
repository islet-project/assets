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
	.align 2
console_base: .word 0x0

	/* -----------------------------------------------
	 * int console_init(uintptr_t base_addr,
	 * unsigned int uart_clk, unsigned int baud_rate)
	 * Function to initialize the console without a
	 * C Runtime to print debug information. It saves
	 * the console base to the data section.
	 * In: r0 - Console base address
	 *     r1 - Uart clock in Hz
	 *     r2 - Baud rate
	 * Out: r0 - Return 1 on success, 0 on error.
	 * Clobber list : r1 - r3
	 * -----------------------------------------------
	 */
func console_init
	ldr	r3, =console_base
	str	r0, [r3]
	b	console_core_init
endfunc console_init

	/* -----------------------------------------------
	 * int console_core_init(uintptr_t base_addr,
	 * unsigned int uart_clk, unsigned int baud_rate)
	 * Function to initialize the console without a
	 * C Runtime to print debug information. This
	 * function will be accessed by console_init and
	 * crash reporting.
	 * In: r0 - Console base address
	 *     r1 - Uart clock in Hz
	 *     r2 - Baud rate
	 * Out: r0 - Return 1 on success, 0 on error.
	 * Clobber list : r1 - r3
	 * -----------------------------------------------
	 */
func console_core_init
	/* Check the input base address */
	cmp	r0, #0
	beq	core_init_fail
	/* Check baud rate and uart clock for sanity */
	cmp	r1, #0
	beq	core_init_fail
	cmp	r2, #0
	beq	core_init_fail
	/* Disable the UART before initialization */
	ldr	r3, [r0, #UARTCR]
	bic	r3, r3, #PL011_UARTCR_UARTEN
	str	r3, [r0, #UARTCR]
	/* Program the baudrate */
	/* Divisor =  (Uart clock * 4) / baudrate */
	lsl	r1, r1, #2
	udiv	r2, r1, r2
	/* IBRD = Divisor >> 6 */
	lsr	r1, r2, #6
	/* Write the IBRD */
	str	r1, [r0, #UARTIBRD]
	/* FBRD = Divisor & 0x3F */
	and	r1, r2, #0x3f
	/* Write the FBRD */
	str	r1, [r0, #UARTFBRD]
	mov	r1, #PL011_LINE_CONTROL
	str	r1, [r0, #UARTLCR_H]
	/* Clear any pending errors */
	mov	r1, #0
	str	r1, [r0, #UARTECR]
	/* Enable tx, rx, and uart overall */
	ldr	r1, =(PL011_UARTCR_RXE | PL011_UARTCR_TXE | PL011_UARTCR_UARTEN)
	str	r1, [r0, #UARTCR]
	mov	r0, #1
	bx	lr
core_init_fail:
	mov	r0, #0
	bx	lr
endfunc console_core_init

	/* ---------------------------------------------
	 * int console_putc(int c)
	 * Function to output a character over the
	 * console. It returns the character printed on
	 * success or an error code.
	 * In : r0 - Character to be printed
	 * Out : r0 - Input character or error code.
	 * Clobber list : r1, r2
	 * ---------------------------------------------
	 */
func console_putc
	ldr	r1, =console_base
	ldr	r1, [r1]
	b	console_core_putc
endfunc console_putc

	/* --------------------------------------------------------
	 * int console_core_putc(int c, uintptr_t base_addr)
	 * Function to output a character over the console. It
	 * returns the character printed on success or an error
	 * code.
	 * In : r0 - Character to be printed
	 *      r1 - Console base address
	 * Out : r0 - Input character or error code.
	 * Clobber list : r2
	 * --------------------------------------------------------
	 */
func console_core_putc
	/* Check the input parameter */
	cmp	r1, #0
	beq	putc_error
	/* Prepend '\r' to '\n' */
	cmp	r0, #0xA
	bne	2f
1:
	/* Check if the transmit FIFO is full */
	ldr	r2, [r1, #UARTFR]
	tst	r2, #PL011_UARTFR_TXFF
	bne	1b
	mov	r2, #0xD
	str	r2, [r1, #UARTDR]
2:
	/* Check if the transmit FIFO is full */
	ldr	r2, [r1, #UARTFR]
	tst	r2, #PL011_UARTFR_TXFF
	bne	2b

	/* Only write 8 bits */
	and	r0, r0, #0xFF
	str	r0, [r1, #UARTDR]
	bx	lr
putc_error:
	mov	r0, #ERROR_NO_VALID_CONSOLE
	bx	lr
endfunc console_core_putc

	/* ---------------------------------------------
	 * int console_getc(void)
	 * Function to get a character from the console.
	 * It returns the character grabbed on success
	 * or an error code on error. This function is
	 * blocking, it waits until there is an
	 * available character to return.
	 * Out : r0 - Return character or error code.
	 * Clobber list : r0 - r3
	 * ---------------------------------------------
	 */
func console_getc
	ldr	r2, =console_base
	ldr	r2, [r2]
	mov	r3, lr

	/* Loop until it returns a character or an error. */
1:	mov	r0, r2
	bl	console_core_getc
	cmp	r0, #ERROR_NO_PENDING_CHAR
	beq	1b

	bx	r3
endfunc console_getc

	/* ---------------------------------------------
	 * int console_try_getc(void)
	 * Function to get a character from the console.
	 * It returns the character grabbed on success
	 * or an error code on error. This function is
	 * non-blocking, it returns immediately.
	 * Out : r0 - Return character or error code.
	 * Clobber list : r0, r1
	 * ---------------------------------------------
	 */
func console_try_getc
	ldr	r0, =console_base
	ldr	r0, [r0]
	b	console_core_getc
endfunc console_try_getc

	/* ---------------------------------------------
	 * int console_core_getc(uintptr_t base_addr)
	 * Function to get a character from the console.
	 * It returns the character grabbed on success
	 * or an error code.
	 * In : r0 - Console base address
	 * Out : r0 - Return character or error code.
	 * Clobber list : r0, r1
	 * ---------------------------------------------
	 */
func console_core_getc
	cmp	r0, #0
	beq	getc_error

	/* Check if the receive FIFO is empty */
	ldr	r1, [r0, #UARTFR]
	tst	r1, #PL011_UARTFR_RXFE
	bne	getc_empty

	/* Read a character from the FIFO */
	ldr	r1, [r0, #UARTDR]
	/* Mask out error flags */
	and	r0, r1, #0xFF
	bx	lr

getc_empty:
	mov	r0, #ERROR_NO_PENDING_CHAR
	bx	lr
getc_error:
	mov	r0, #ERROR_NO_VALID_CONSOLE
	bx	lr
endfunc console_core_getc

	/* ---------------------------------------------
	 * int console_flush(void)
	 * Function to force a write of all buffered
	 * data that hasn't been output. It returns 0
	 * upon successful completion, otherwise it
	 * returns an error code.
	 * Out: r0 - Error code or 0.
	 * Clobber list : r0, r1
	 * ---------------------------------------------
	 */
func console_flush
	ldr	r0, =console_base
	ldr	r0, [r0]
	b	console_core_flush
endfunc console_flush

	/* ---------------------------------------------
	 * int console_core_flush(uintptr_t base_addr)
	 * Function to force a write of all buffered
	 * data that hasn't been output.
	 * In : r0 - Console base address
	 * Out : r0 - Error code or 0.
	 * Clobber list : r0, r1
	 * ---------------------------------------------
	 */
func console_core_flush
	cmp	r0, #0
	beq	flush_error

1:
	/* Loop while the transmit FIFO is busy */
	ldr	r1, [r0, #UARTFR]
	tst	r1, #PL011_UARTFR_BUSY
	bne	1b

	mov	r0, #0
	bx	lr
flush_error:
	mov	r0, #ERROR_NO_VALID_CONSOLE
	bx	lr
endfunc console_core_flush
