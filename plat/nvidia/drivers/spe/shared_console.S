/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <drivers/console.h>

#define CONSOLE_NUM_BYTES_SHIFT		24
#define CONSOLE_FLUSH_DATA_TO_PORT	(1 << 26)
#define CONSOLE_RING_DOORBELL		(1 << 31)
#define CONSOLE_IS_BUSY			(1 << 31)
#define CONSOLE_TIMEOUT			0xC000		/* 50 ms */

	/*
	 * This file contains a driver implementation to make use of the
	 * real console implementation provided by the SPE firmware running
	 * SoCs after Tegra186.
	 *
	 * This console is shared by multiple components and the SPE firmware
	 * finally displays everything on the UART port.
	 */

	.globl	console_init
	.globl	console_putc
	.globl	console_getc
	.globl	console_try_getc
	.globl	console_flush

.macro	check_if_console_is_ready base, tmp1, tmp2, label
	/* wait until spe is ready or timeout expires */
1:	ldr	\tmp1, [\base]
	and	\tmp1, \tmp1, #CONSOLE_IS_BUSY
	cbnz	\tmp1, 1b
.endm

	/*
	 *  The console base is in the data section and not in .bss
	 *  even though it is zero-init. In particular, this allows
	 *  the console functions to start using this variable before
	 *  the runtime memory is initialized for images which do not
	 *  need to copy the .data section from ROM to RAM.
	 */
	.section .data.console_base
	.align 3
console_base:
	.quad 0x0

	/* -----------------------------------------------
	 * int console_init(uintptr_t base_addr,
	 *		    unsigned int uart_clk,
	 *		    unsigned int baud_rate)
	 *
	 * Clobber list : x1 - x3
	 * -----------------------------------------------
	 */
func console_init
	/* Check the input base address */
	cbz	x0, register_fail

	/* save base address */
	adr	x3, console_base
	str	x0, [x3]

	/* Dont use clock or baud rate, so ok to overwrite them */
	check_if_console_is_ready x0, x1, x2, register_fail

register_fail:
	mov	w0, wzr
	ret
endfunc console_init

	/* --------------------------------------------------------
	 * int console_spe_core_putc(int c, uintptr_t base_addr)
	 * Function to output a character over the console. It
	 * returns the character printed on success or -1 on error.
	 * In : w0 - character to be printed
	 *      x1 - console base address
	 * Out : return -1 on error else return character.
	 * Clobber list : x2, x3
	 * --------------------------------------------------------
	 */
func console_spe_core_putc
	/* Check the input parameter */
	cbz	x1, putc_error

	/* Prepend '\r' to '\n' */
	cmp	w0, #0xA
	b.ne	not_eol

	check_if_console_is_ready x1, x2, x3, putc_error

	/* spe is ready */
	mov	w2, #0xD		/* '\r' */
	and	w2, w2, #0xFF
	mov	w3, #(CONSOLE_RING_DOORBELL | (1 << CONSOLE_NUM_BYTES_SHIFT))
	orr	w2, w2, w3
	str	w2, [x1]

not_eol:
	check_if_console_is_ready x1, x2, x3, putc_error

	/* spe is ready */
	mov	w2, w0
	and	w2, w2, #0xFF
	mov	w3, #(CONSOLE_RING_DOORBELL | (1 << CONSOLE_NUM_BYTES_SHIFT))
	orr	w2, w2, w3
	str	w2, [x1]

	ret
putc_error:
	mov	w0, #-1
	ret
endfunc console_spe_core_putc

	/* ---------------------------------------------
	 * int console_putc(int c)
	 *
	 * Clobber list : x1, x2
	 * ---------------------------------------------
	 */
func console_putc
	adr	x1, console_base
	ldr	x1, [x1]
	b	console_spe_core_putc
endfunc console_putc

	/* ---------------------------------------------
	 * int console_getc(void)
	 *
	 * Clobber list : x0 - x3
	 * ---------------------------------------------
	 */
func console_getc
	mov	w0, #-1
	ret
endfunc console_getc

	/* ---------------------------------------------
	 * int console_try_getc(void)
	 *
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_try_getc
	mov	w0, #-1
	ret
endfunc console_try_getc

	/* -------------------------------------------------
	 * int console_spe_core_flush(uintptr_t base_addr)
	 * Function to force a write of all buffered
	 * data that hasn't been output.
	 * In : x0 - console base address
	 * Out : return -1 on error else return 0.
	 * Clobber list : x0, x1
	 * -------------------------------------------------
	 */
func console_spe_core_flush
	cbz	x0, flush_error

	/* flush console */
	mov	w1, #(CONSOLE_RING_DOORBELL | CONSOLE_FLUSH_DATA_TO_PORT)
	str	w1, [x0]
	mov	w0, #0
	ret
flush_error:
	mov	w0, #-1
	ret
endfunc console_spe_core_flush

	/* ---------------------------------------------
	 * int console_flush(void)
	 *
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_flush
	adr	x0, console_base
	ldr	x0, [x0]
	b	console_spe_core_flush
endfunc console_flush
