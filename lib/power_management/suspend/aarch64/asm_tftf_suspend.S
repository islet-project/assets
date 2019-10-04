/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <psci.h>
#include "../suspend_private.h"

	.global __tftf_suspend
	.global __tftf_save_arch_context
	.global __tftf_cpu_resume_ep

	.section	.text, "ax"

/*
 * Saves CPU state for entering suspend. This saves callee registers on stack,
 * and allocates space on the stack to save the CPU specific registers for
 * coming out of suspend.
 *
 *  x0 contains a pointer to tftf_suspend_context structure.
 */
func __tftf_suspend
	stp	x29, x30, [sp, #-96]!
	stp	x19, x20, [sp, #16]
	stp	x21, x22, [sp, #32]
	stp	x23, x24, [sp, #48]
	stp	x25, x26, [sp, #64]
	stp	x27, x28, [sp, #80]
	mov	x2, sp
	sub	sp, sp, #SUSPEND_CTX_SZ
	mov	x1, sp
        /*
         * x1 now points to struct tftf_suspend_ctx allocated on the stack
         */
	str	x2, [x1, #SUSPEND_CTX_SP_OFFSET]
	bl	tftf_enter_suspend

	/*
	 * If execution reaches this point, the suspend call was either
	 * a suspend to standby call or an invalid suspend call.
	 * In case of suspend to powerdown, execution will instead resume in
	 * __tftf_cpu_resume_ep().
	 */
	add	sp, sp, #SUSPEND_CTX_SZ
	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	x25, x26, [sp, #64]
	ldp	x27, x28, [sp, #80]
	ldp	x29, x30, [sp], #96
	ret
endfunc __tftf_suspend

func __tftf_save_arch_context
#if ENABLE_PAUTH
	mrs     x1, APIAKeyLo_EL1
	mrs     x2, APIAKeyHi_EL1
	stp	x1, x2, [x0, #SUSPEND_CTX_APIAKEY_OFFSET]
#endif
	JUMP_EL1_OR_EL2 x1, 1f, 2f, dead
1:	mrs	x1, mair_el1
	mrs	x2, cpacr_el1
	mrs	x3, ttbr0_el1
	mrs	x4, tcr_el1
	mrs	x5, vbar_el1
	mrs	x6, sctlr_el1
	stp	x1, x2, [x0, #SUSPEND_CTX_MAIR_OFFSET]
	stp	x3, x4, [x0, #SUSPEND_CTX_TTBR0_OFFSET]
	stp	x5, x6, [x0, #SUSPEND_CTX_VBAR_OFFSET]
	ret

2:	mrs	x1, mair_el2
	mrs	x2, hcr_el2
	mrs	x3, ttbr0_el2
	mrs	x4, tcr_el2
	mrs	x5, vbar_el2
	mrs	x6, sctlr_el2
	stp	x1, x2, [x0, #SUSPEND_CTX_MAIR_OFFSET]
	stp	x3, x4, [x0, #SUSPEND_CTX_TTBR0_OFFSET]
	stp	x5, x6, [x0, #SUSPEND_CTX_VBAR_OFFSET]
	ret
endfunc __tftf_save_arch_context

/*
 * Restore CPU register context
 * X0  -- Should contain the context pointer
 */
func __tftf_cpu_resume_ep
	JUMP_EL1_OR_EL2 x1, 1f, 2f, dead
1:	/* Invalidate local tlb entries before turning on MMU */
	tlbi	vmalle1
	ldp	x1, x2, [x0, #SUSPEND_CTX_MAIR_OFFSET]
	ldp	x3, x4, [x0, #SUSPEND_CTX_TTBR0_OFFSET]
	ldp	x5, x6, [x0, #SUSPEND_CTX_VBAR_OFFSET]
	msr	mair_el1, x1
	msr	cpacr_el1, x2
	msr	ttbr0_el1, x3
	msr	tcr_el1, x4
	msr	vbar_el1, x5
	/*
	 * TLB invalidations need to be completed before enabling MMU
	 */
	dsb	nsh
	msr	sctlr_el1, x6
	/* Ensure the MMU enable takes effect immediately */
	isb
	b	restore_callee_regs

	/* Invalidate local tlb entries before turning on MMU */
2:	tlbi	alle2
	ldp	x1, x2, [x0, #SUSPEND_CTX_MAIR_OFFSET]
	ldp	x3, x4, [x0, #SUSPEND_CTX_TTBR0_OFFSET]
	ldp	x5, x6, [x0, #SUSPEND_CTX_VBAR_OFFSET]
	msr	mair_el2, x1
	msr	hcr_el2, x2
	msr	ttbr0_el2, x3
	msr	tcr_el2, x4
	msr	vbar_el2, x5
	/*
	 * TLB invalidations need to be completed before enabling MMU
	 */
	dsb	nsh
	msr	sctlr_el2, x6
	/* Ensure the MMU enable takes effect immediately */
	isb

restore_callee_regs:
#if ENABLE_PAUTH
	ldp	x1, x2, [x0, #SUSPEND_CTX_APIAKEY_OFFSET]
	msr     APIAKeyLo_EL1, x1
	msr     APIAKeyHi_EL1, x2
#endif
	ldr	x2, [x0, #SUSPEND_CTX_SP_OFFSET]
	mov	sp, x2
	ldr	w1, [x0, #SUSPEND_CTX_SAVE_SYSTEM_CTX_OFFSET]
	cbz	w1, skip_sys_restore
	bl	tftf_restore_system_ctx
skip_sys_restore:
	ldp	x19, x20, [sp, #16]	/* Restore the callee saved registers */
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	x25, x26, [sp, #64]
	ldp	x27, x28, [sp, #80]
	ldp	x29, x30, [sp], #96
	mov	x0, PSCI_E_SUCCESS
	ret
endfunc __tftf_cpu_resume_ep

dead:
	b	.
