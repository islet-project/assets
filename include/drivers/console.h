/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
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

#include <types.h>

int console_init(uintptr_t base_addr,
		unsigned int uart_clk, unsigned int baud_rate);
int console_putc(int c);
int console_getc(void);
int console_try_getc(void);
int console_flush(void);

#endif /* __ASSEMBLY__ */

#endif /* __CONSOLE_H__ */
