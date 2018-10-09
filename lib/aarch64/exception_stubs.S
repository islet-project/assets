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
vector_entry SynchronousExceptionSP0
	b	.
vector_entry IrqSP0
	b	.
vector_entry FiqSP0
	b	.
vector_entry SErrorSP0
	b	.
vector_entry SynchronousExceptionSPx
	b	.
vector_entry IrqSPx
	b	.
vector_entry FiqSPx
	b	.
vector_entry SErrorSPx
	b	.
vector_entry SynchronousExceptionA64
	b	.
vector_entry IrqA64
	b	.
vector_entry FiqA64
	b	.
vector_entry SErrorA64
	b	.
vector_entry SynchronousExceptionA32
	b	.
vector_entry IrqA32
	b	.
vector_entry FiqA32
	b	.
vector_entry SErrorA32
	b	.
