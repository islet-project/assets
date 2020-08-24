/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.global exception_stubs

/*
 * Simplistic exceptions vector table.
 * All entries spin, which means all types of exceptions are unrecoverable.
 */

vector_base exception_stubs
vector_entry_spin sync_exception_sp_el0
vector_entry_spin irq_sp_el0
vector_entry_spin fiq_sp_el0
vector_entry_spin serror_sp_el0
vector_entry_spin sync_exception_sp_elx
vector_entry_spin irq_sp_elx
vector_entry_spin fiq_sp_elx
vector_entry_spin serror_sp_elx
vector_entry_spin sync_exception_aarch64
vector_entry_spin irq_aarch64
vector_entry_spin fiq_aarch64
vector_entry_spin serror_aarch64
vector_entry_spin sync_exception_aarch32
vector_entry_spin irq_aarch32
vector_entry_spin fiq_aarch32
vector_entry_spin serror_aarch32
