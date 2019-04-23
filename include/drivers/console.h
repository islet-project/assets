/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

/* Returned by getc callbacks when receive FIFO is empty. */
#define ERROR_NO_PENDING_CHAR		-1
/* Returned by console_xxx() if the registered console doesn't implement xxx. */
#define ERROR_NO_VALID_CONSOLE		(-128)

#ifndef __ASSEMBLY__

#include <stdint.h>

/*
 * Function to initialize the console without a C Runtime to print debug
 * information. It saves the console base to the data section. Returns 1 on
 * success, 0 on error.
 */
int console_init(uintptr_t base_addr,
		unsigned int uart_clk, unsigned int baud_rate);

/*
 * Function to output a character over the console. It returns the character
 * printed on success or an error code.
 */
int console_putc(int c);

/*
 * Function to get a character from the console.  It returns the character
 * grabbed on success or an error code on error. This function is blocking, it
 * waits until there is an available character to return. Returns a character or
 * error code.
 */
int console_getc(void);

/*
 * Function to get a character from the console.  It returns the character
 * grabbed on success or an error code on error. This function is non-blocking,
 * it returns immediately.
 */
int console_try_getc(void);

/*
 * Function to force a write of all buffered data that hasn't been output. It
 * returns 0 upon successful completion, otherwise it returns an error code.
 */
int console_flush(void);

#endif /* __ASSEMBLY__ */

#endif /* __CONSOLE_H__ */
