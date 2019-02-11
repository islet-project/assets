/*
 * Copyright (c) 2013-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <cdefs.h>
#include <stdio.h>

#include <common/debug.h>

void __assert(const char *file, unsigned int line, const char *assertion)
{
	printf("ASSERT: %s:%d:%s\n", file, line, assertion);
	panic();
}
