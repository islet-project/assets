/*
 * Copyright (c) 2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>

/* -----------------------------------------------------------------------------
 * Very simple stackless exception handlers used by the spm shim layer.
 * -----------------------------------------------------------------------------
 */
	.globl	spm_shim_exceptions_ptr

vector_base spm_shim_exceptions_ptr

	/* -----------------------------------------------------
	 * Current EL with SP0 : 0x0 - 0x200
	 * -----------------------------------------------------
	 */
vector_entry_spin sync_exception_sp_el0

vector_entry_spin irq_sp_el0

vector_entry_spin fiq_ep_el0

vector_entry_spin serror_ep_el0

	/* -----------------------------------------------------
	 * Current EL with SPx: 0x200 - 0x400
	 * -----------------------------------------------------
	 */
vector_entry_spin sync_exception_sp_elx

vector_entry_spin irq_sp_elx

vector_entry_spin fiq_sp_elx

vector_entry_spin serror_sp_elx

	/* -----------------------------------------------------
	 * Lower EL using AArch64 : 0x400 - 0x600. No exceptions
	 * are handled since secure_partition does not implement
	 * a lower EL
	 * -----------------------------------------------------
	 */
vector_entry sync_exception_aarch64
	msr	tpidr_el1, x30
	mrs	x30, esr_el1
	ubfx	x30, x30, #ESR_EC_SHIFT, #ESR_EC_LENGTH

	cmp	x30, #EC_AARCH64_SVC
	b.eq 	do_smc

	cmp	x30, #EC_AARCH32_SVC
	b.eq	do_smc

	cmp	x30, #EC_AARCH64_SYS
	b.eq	handle_sys_trap

	/* Fail in all the other cases */
	b	panic

	/* ---------------------------------------------
	 * Tell SPM that we are done initialising
	 * ---------------------------------------------
	 */
do_smc:
	mrs	x30, tpidr_el1
	smc	#0
	eret

	/* AArch64 system instructions trap are handled as a panic for now */
handle_sys_trap:
panic:
	b	panic
end_vector_entry sync_exception_aarch64

vector_entry_spin irq_aarch64

vector_entry_spin fiq_aarch64

vector_entry_spin serror_aarch64

	/* -----------------------------------------------------
	 * Lower EL using AArch32 : 0x600 - 0x800
	 * -----------------------------------------------------
	 */
vector_entry_spin sync_exception_aarch32

vector_entry_spin irq_aarch32

vector_entry_spin fiq_aarch32

vector_entry_spin serror_aarch32
