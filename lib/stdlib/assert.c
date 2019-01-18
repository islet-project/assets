/*
 * Copyright (c) 2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

/*
 * This is a basic implementation. This could be improved.
 */
void __assert (const char *function, const char *file, unsigned int line,
		const char *assertion)
{
	printf("ASSERT: %s <%d> : %s\n", function, line, assertion);

	while (1)
		;
}
