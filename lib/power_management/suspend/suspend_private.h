/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SUSPEND_PRIV_H__
#define __SUSPEND_PRIV_H__

#define SUSPEND_CTX_SZ 64
#define SUSPEND_CTX_SP_OFFSET 48
#define SUSPEND_CTX_SAVE_SYSTEM_CTX_OFFSET 56

#ifndef __ASSEMBLY__
#include <cassert.h>
#include <power_management.h>
#include <stdint.h>
#include <string.h>
#include <types.h>

#define NR_CTX_REGS 6

/*
 * struct tftf_suspend_ctx represents the architecture context to
 * be saved and restored while entering suspend and coming out.
 * It must be 16-byte aligned since it is allocated on the stack, which must be
 * 16-byte aligned on ARMv8 (AArch64). Even though the alignment requirement
 * is not present in AArch32, we use the same alignment and register width as
 * it allows the same structure to be reused for AArch32.
 */
typedef struct tftf_suspend_context {
	uint64_t arch_ctx_regs[NR_CTX_REGS];
	uint64_t stack_pointer;
	/*
	 * Whether the system context is saved and and needs to be restored.
	 * Note that the system context itself is not saved in this structure.
	 */
	unsigned int save_system_context;
} __aligned(16) tftf_suspend_ctx_t;

/*
 * Saves callee save registers on the stack
 * Allocate space on stack for CPU context regs
 * Enters suspend by calling tftf_enter_suspend.
 *     power state: PSCI power state to be sent via SMC
 *     Returns: PSCI_E_SUCCESS or PSCI_E_INVALID_PARAMS
 */
unsigned int __tftf_suspend(const suspend_info_t *power_state);

/*
 * Saves the architecture context of CPU in the memory
 *     tftf_suspend_context: Pointer to the location for saving the context
 */
void __tftf_save_arch_context(struct tftf_suspend_context *ctx);

/*
 * Calls __tftf_save_arch_context to saves arch context of cpu to the memory
 * pointed by ctx
 * Enters suspend by calling the SMC
 *     power state: PSCI power state to be sent via SMC
 *     ctx: Pointer to the location where suspend context can be stored
 *     Returns: PSCI_E_SUCCESS or PSCI_E_INVALID_PARAMS
 */
int32_t tftf_enter_suspend(const suspend_info_t *power_state,
			   tftf_suspend_ctx_t *ctx);

/*
 * Invokes the appropriate driver functions in the TFTF framework
 * to save their context prior to a system suspend.
 */
void tftf_save_system_ctx(tftf_suspend_ctx_t *ctx);

/*
 * Invokes the appropriate driver functions in the TFTF framework
 * to restore their context on wake-up from system suspend.
 */
void tftf_restore_system_ctx(tftf_suspend_ctx_t *ctx);

/*
 * Restores the CPU arch context and callee registers from the location pointed
 * by X0(context ID).
 * Returns: PSCI_E_SUCCESS
 */
unsigned int __tftf_cpu_resume_ep(void);

/* Assembler asserts to verify #defines of offsets match as seen by compiler */
CASSERT(SUSPEND_CTX_SZ == sizeof(tftf_suspend_ctx_t),
				assert_suspend_context_size_mismatch);
CASSERT(SUSPEND_CTX_SP_OFFSET == __builtin_offsetof(tftf_suspend_ctx_t, stack_pointer),
			assert_stack_pointer_location_mismatch_in_suspend_ctx);
CASSERT(SUSPEND_CTX_SAVE_SYSTEM_CTX_OFFSET ==
		__builtin_offsetof(tftf_suspend_ctx_t, save_system_context),
			assert_save_sys_ctx_mismatch_in_suspend_ctx);
#endif	/* __ASSEMBLY__ */

#endif	/* __SUSPEND_PRIV_H__ */
