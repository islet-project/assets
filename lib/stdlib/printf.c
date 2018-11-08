/*
 * Copyright (c) 2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdarg.h>
#include <stdio.h>

/* Choose max of 512 chars for now. */
#define PRINT_BUFFER_SIZE 512

int vprintf(const char *fmt, va_list args)
{
	char buf[PRINT_BUFFER_SIZE];
	int count;

	vsnprintf(buf, sizeof(buf) - 1, fmt, args);
	buf[PRINT_BUFFER_SIZE - 1] = '\0';

	/* Use putchar directly as 'puts()' adds a newline. */
	count = 0;
	while (buf[count] != 0) {
		if (putchar(buf[count]) == EOF) {
			return EOF;
		}
		count++;
	}

	return count;
}

int printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	int count = vprintf(fmt, args);
	va_end(args);

	return count;
}
