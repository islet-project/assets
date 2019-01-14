/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.global exception_stubs

/*
 * Simplistic exceptions vector table.
 * All entries spin, which means all types of exceptions are unrecoverable.
 */
	.macro vector_entry_spin name
	vector_entry \name
	b \name
	end_vector_entry \name
	.endm

vector_base exception_stubs
vector_entry_spin SynchronousExceptionSP0
vector_entry_spin IrqSP0
vector_entry_spin FiqSP0
vector_entry_spin SErrorSP0
vector_entry_spin SynchronousExceptionSPx
vector_entry_spin IrqSPx
vector_entry_spin FiqSPx
vector_entry_spin SErrorSPx
vector_entry_spin SynchronousExceptionA64
vector_entry_spin IrqA64
vector_entry_spin FiqA64
vector_entry_spin SErrorA64
vector_entry_spin SynchronousExceptionA32
vector_entry_spin IrqA32
vector_entry_spin FiqA32
vector_entry_spin SErrorA32
