/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

/*
 * Simplistic exceptions vector table.
 * All entries spin, which means all types of exceptions are unrecoverable.
 */
	.global exception_stubs
vector_base exception_stubs
	b	.		/* Not used */
	b	.		/* Undef */
	b	.		/* Syscall */
	b	.		/* Prefetch abort */
	b	.		/* Data abort */
	b	.		/* Hyp trap */
	b	.		/* IRQ */
	b	.		/* FIQ */
