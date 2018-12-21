/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <spinlock.h>
#include <stdarg.h>
#include <stdio.h>

/* Lock to avoid concurrent accesses to the serial console */
static spinlock_t printf_lock;

void mp_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	spin_lock(&printf_lock);
	vprintf(fmt, args);
	spin_unlock(&printf_lock);

	va_end(args);
}
