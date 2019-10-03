/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SUSPEND_PRIV_H__
#define __SUSPEND_PRIV_H__

/*
 * Number of system registers we need to save/restore across a CPU suspend:
 * MAIR, CPACR_EL1/HCR_EL2, TTBR0, TCR, VBAR, SCTLR,
 * APIAKeyLo_EL1 and APIAKeyHi_EL1 (if enabled).
 */
#if ENABLE_PAUTH
#define NR_CTX_REGS 8
#else
#define NR_CTX_REGS 6
#endif

/* Offsets of the fields in the context structure. Needed by asm code. */
#define	SUSPEND_CTX_MAIR_OFFSET		0
#define	SUSPEND_CTX_TTBR0_OFFSET	16
#define	SUSPEND_CTX_VBAR_OFFSET		32
#define	SUSPEND_CTX_APIAKEY_OFFSET	48

#define SUSPEND_CTX_SP_OFFSET (8 * NR_CTX_REGS)
#define SUSPEND_CTX_SAVE_SYSTEM_CTX_OFFSET (SUSPEND_CTX_SP_OFFSET + 8)

/*
 * Size of the context structure.
 * +8 because of the padding bytes inserted for alignment constraint.
 */
#define SUSPEND_CTX_SZ (SUSPEND_CTX_SAVE_SYSTEM_CTX_OFFSET + 8)

#ifndef __ASSEMBLY__
#include <cassert.h>
#include <power_management.h>
#include <stdint.h>
#include <string.h>

/*
 * Architectural context to be saved/restored when entering/exiting suspend
 * mode.
 *
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
 * Ensure consistent view of the context structure layout across asm and C
 * code.
 */
CASSERT(SUSPEND_CTX_SZ == sizeof(tftf_suspend_ctx_t),
	assert_suspend_context_size_mismatch);

CASSERT(SUSPEND_CTX_SP_OFFSET ==
	__builtin_offsetof(tftf_suspend_ctx_t, stack_pointer),
	assert_stack_pointer_location_mismatch_in_suspend_ctx);

CASSERT(SUSPEND_CTX_SAVE_SYSTEM_CTX_OFFSET ==
	__builtin_offsetof(tftf_suspend_ctx_t, save_system_context),
	assert_save_sys_ctx_mismatch_in_suspend_ctx);

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

#endif	/* __ASSEMBLY__ */

#endif	/* __SUSPEND_PRIV_H__ */
