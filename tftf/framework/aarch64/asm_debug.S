/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>

#if ENABLE_ASSERTIONS

	.globl	asm_assert

/* Since the max decimal input number is 65536 */
#define MAX_DEC_DIVISOR		10000
/* The offset to add to get ascii for numerals '0 - 9' */
#define ASCII_OFFSET_NUM	0x30

.section .rodata.assert_str, "aS"
assert_msg1:
	.asciz "ASSERT: File "
assert_msg2:
	.asciz " Line "

	/*
	 * This macro is intended to be used to print the
	 * line number in decimal. Used by asm_assert macro.
	 * The max number expected is 65536.
	 * In: x4 = the decimal to print.
	 * Clobber: x30, x0, x1, x2, x5, x6
	 */
	.macro asm_print_line_dec
	mov	x6, #10		/* Divide by 10 after every loop iteration */
	mov	x5, #MAX_DEC_DIVISOR
dec_print_loop:
	udiv	x0, x4, x5		/* Get the quotient */
	msub	x4, x0, x5, x4		/* Find the remainder */
	add	x0, x0, #ASCII_OFFSET_NUM		/* Convert to ascii */
	bl	plat_crash_console_putc
	udiv	x5, x5, x6		/* Reduce divisor */
	cbnz	x5, dec_print_loop
	.endm

/* ---------------------------------------------------------------------------
 * Assertion support in assembly.
 * The below function helps to support assertions in assembly where we do not
 * have a C runtime stack. Arguments to the function are :
 * x0 - File name
 * x1 - Line no
 * Clobber list : x30, x0, x1, x2, x3, x4, x5, x6.
 * ---------------------------------------------------------------------------
 */
func asm_assert
	mov	x5, x0
	mov	x6, x1
	/* Ensure the console is initialized */
	bl	plat_crash_console_init
	/* Check if the console is initialized */
	cbz	x0, _assert_loop
	/* The console is initialized */
	adr	x4, assert_msg1
	bl	asm_print_str
	mov	x4, x5
	bl	asm_print_str
	adr	x4, assert_msg2
	bl	asm_print_str
	/* Check if line number higher than max permitted */
	tst	x6, #~0xffff
	b.ne	_assert_loop
	mov	x4, x6
	asm_print_line_dec
	bl	plat_crash_console_flush
_assert_loop:
	wfi
	b	_assert_loop
endfunc asm_assert

/*
 * This function prints a string from address in x4.
 * In: x4 = pointer to string.
 * Clobber: x30, x0, x1, x2, x3
 */
func asm_print_str
	mov	x3, x30
1:
	ldrb	w0, [x4], #0x1
	cbz	x0, 2f
	bl	plat_crash_console_putc
	b	1b
2:
	ret	x3
endfunc asm_print_str

/*
 * This function prints a hexadecimal number in x4.
 * In: x4 = the hexadecimal to print.
 * Clobber: x30, x0 - x3, x5
 */
func asm_print_hex
	mov	x3, x30
	mov	x5, #64  /* No of bits to convert to ascii */
1:
	sub	x5, x5, #4
	lsrv	x0, x4, x5
	and	x0, x0, #0xf
	cmp	x0, #0xA
	b.lo	2f
	/* Add by 0x27 in addition to ASCII_OFFSET_NUM
	 * to get ascii for characters 'a - f'.
	 */
	add	x0, x0, #0x27
2:
	add	x0, x0, #ASCII_OFFSET_NUM
	bl	plat_crash_console_putc
	cbnz	x5, 1b
	ret	x3
endfunc asm_print_hex

#endif /* ENABLE_ASSERTIONS */
