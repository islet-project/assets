/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
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
 * r0 contains a pointer to tftf_suspend_context structure.
 */
func __tftf_suspend
	push	{r4 - r12, lr}
	mov	r2, sp
	sub	sp, sp, #SUSPEND_CTX_SZ
	mov	r1, sp
        /*
         * r1 now points to struct tftf_suspend_ctx allocated on the stack
         */
	str	r2, [r1, #SUSPEND_CTX_SP_OFFSET]
	bl	tftf_enter_suspend

	/*
	 * If execution reaches this point, the suspend call was either
	 * a suspend to standby call or an invalid suspend call.
	 * In case of suspend to powerdown, execution will instead resume in
	 * __tftf_cpu_resume_ep().
	 */
	add	sp, sp, #SUSPEND_CTX_SZ
	pop	{r4 - r12, lr}
	bx	lr
endfunc __tftf_suspend

func __tftf_save_arch_context
	ldcopr	r1, HMAIR0
	ldcopr	r2, HCR
	stm	r0!, {r1, r2}
	ldcopr16	r1, r2, HTTBR_64
	stm	r0!, {r1, r2}
	ldcopr	r1, HTCR
	ldcopr	r2, HVBAR
	ldcopr	r3, HSCTLR
	stm	r0, {r1, r2, r3}
	bx	lr
endfunc __tftf_save_arch_context

/*
 * Restore CPU register context
 * r0  -- Should contain the context pointer
 */
func __tftf_cpu_resume_ep
	/* Invalidate local tlb entries before turning on MMU */
	stcopr	r0, TLBIALLH
	mov 	r4, r0
	ldm	r0!, {r1, r2}
	stcopr	r1, HMAIR0
	stcopr	r2, HCR
	ldm	r0!, {r1, r2}
	stcopr16	r1, r2, HTTBR_64
	ldm	r0, {r1, r2, r3}
	stcopr	r1, HTCR
	stcopr	r2, HVBAR

	/*
	 * TLB invalidations need to be completed before enabling MMU
	 */
	dsb	nsh
	stcopr	r3, HSCTLR
	/* Ensure the MMU enable takes effect immediately */
	isb

	mov	r0, r4
	ldr	r2, [r0, #SUSPEND_CTX_SP_OFFSET]
	mov	sp, r2
	ldr	r1, [r0, #SUSPEND_CTX_SAVE_SYSTEM_CTX_OFFSET]
	cmp	r1, #0
	beq	skip_sys_restore
	bl	tftf_restore_system_ctx
skip_sys_restore:
	pop	{r4 - r12, lr}
	mov	r0, #PSCI_E_SUCCESS
	bx	lr
endfunc __tftf_cpu_resume_ep
