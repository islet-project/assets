/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "asm_macros.S"
#include "platform.h"

/*
 * PL011 UART registers
 */
/* UART Flag Register */
#define UARTFR			0x018
/* Transmit FIFO full bit in UARTFR register */
#define PL011_UARTFR_TXFF_BIT	5
/* UART Data Register */
#define UARTDR			0x000

	.text
	.global print_string
	.global print_char

	/*
	 * void print_char(unsigned int c);
	 * clobbers: x3, x4
	 */
func print_char
	ldr	x3, =UART_BASE
1:
	/* Check if the transmit FIFO is full */
	ldr	w4, [x3, #UARTFR]
	tbnz	w4, #PL011_UARTFR_TXFF_BIT, 1b
	str	w0, [x3, #UARTDR]
	ret
endfunc print_char

	/*
	 * void print_string(const char *str);
	 * clobbers: x0, x1, x2, x9
	 */
func print_string
	ldr	x1, =UART_BASE
	mov	x2, x0
1:
	ldrb	w0, [x2], #1
	cmp	wzr, w0
	b.eq	2f

	mov	x9, x30
	bl	print_char
	mov	x30, x9
	b	1b
2:
	ret
endfunc print_string
