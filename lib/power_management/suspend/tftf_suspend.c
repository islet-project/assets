/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <arm_gic.h>
#include <console.h>
#include <debug.h>
#include <platform.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <stdint.h>
#include <tftf.h>
#include <tftf_lib.h>
#include "suspend_private.h"

int32_t tftf_enter_suspend(const suspend_info_t *info,
			   tftf_suspend_ctx_t *ctx)
{
	smc_args cpu_suspend_args = {
			info->psci_api,
			info->power_state,
			(uintptr_t)__tftf_cpu_resume_ep,
			(u_register_t)ctx
		};

	smc_args system_suspend_args = {
			info->psci_api,
			(uintptr_t)__tftf_cpu_resume_ep,
			(u_register_t)ctx
		};

	smc_ret_values rc;

	if (info->save_system_context) {
		ctx->save_system_context = 1;
		tftf_save_system_ctx(ctx);
	} else
		ctx->save_system_context = 0;

	/*
	 * Save the CPU context. It will be restored in resume path in
	 * __tftf_cpu_resume_ep().
	 */
	__tftf_save_arch_context(ctx);

	/*
	 * Flush the context that must be retrieved with MMU off
	 */
	flush_dcache_range((u_register_t)ctx, sizeof(*ctx));

	/* Make sure any outstanding message is printed. */
	console_flush();

	if (info->psci_api == SMC_PSCI_CPU_SUSPEND)
		rc = tftf_smc(&cpu_suspend_args);
	else
		rc = tftf_smc(&system_suspend_args);

	/*
	 * If execution reaches this point, The above SMC call was an invalid
	 * call or a suspend to standby call. In both cases the CPU does not
	 * power down so there is no need to restore the context.
	 */
	return rc.ret0;
}

void tftf_restore_system_ctx(tftf_suspend_ctx_t *ctx)
{
	assert(ctx != NULL);
	assert(ctx->save_system_context);

	/*
	 * TODO: Check if there is a need for separate platform
	 * API for resume.
	 */

	tftf_early_platform_setup();

	INFO("Restoring system context\n");

	/* restore the global GIC context */
	arm_gic_restore_context_global();
	tftf_timer_gic_state_restore();
}

void tftf_save_system_ctx(tftf_suspend_ctx_t *ctx)
{
	assert(ctx != NULL);
	assert(ctx->save_system_context);

	/* Nothing to do here currently */
	INFO("Saving system context\n");

	/* Save the global GIC context */
	arm_gic_save_context_global();
}

int tftf_suspend(const suspend_info_t *info)
{
	int32_t rc;
	uint64_t flags;

	flags = read_daif();

	disable_irq();

	INFO("Going into suspend state\n");

	/* Save the local GIC context */
	arm_gic_save_context_local();

	rc = __tftf_suspend(info);

	/* Restore the local GIC context */
	arm_gic_restore_context_local();

	/*
	 * DAIF flags should be restored last because it could be an issue
	 * to unmask exceptions before that point, e.g. if GIC must be
	 * reconfigured upon resume from suspend.
	 */
	write_daif(flags);

	INFO("Resumed from suspend state\n");

	return rc;
}
