/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arm_gic.h>
#include <assert.h>
#include <sdei.h>
#include <smccc.h>
#include <stdint.h>
#include <tftf_lib.h>

int64_t sdei_version(void)
{
	smc_args args = { SDEI_VERSION };
	smc_ret_values ret;

	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_interrupt_bind(int intr, struct sdei_intr_ctx *intr_ctx)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	assert(intr_ctx);

	intr_ctx->priority = arm_gic_get_intr_priority(intr);
	intr_ctx->num = intr;
	intr_ctx->enabled = arm_gic_intr_enabled(intr);
	arm_gic_intr_disable(intr);

	args.fid = SDEI_INTERRUPT_BIND;
	args.arg1 = intr;
	ret = tftf_smc(&args);
	if (ret.ret0 < 0) {
		arm_gic_set_intr_priority(intr_ctx->num, intr_ctx->priority);
		if (intr_ctx->enabled)
			arm_gic_intr_enable(intr_ctx->num);
	}

	return ret.ret0;
}

int64_t sdei_interrupt_release(int ev, const struct sdei_intr_ctx *intr_ctx)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	assert(intr_ctx);

	args.fid = SDEI_INTERRUPT_RELEASE;
	args.arg1 = ev;
	ret = tftf_smc(&args);
	if (ret.ret0 == 0) {
		arm_gic_set_intr_priority(intr_ctx->num, intr_ctx->priority);
		if (intr_ctx->enabled)
			arm_gic_intr_enable(intr_ctx->num);
	}

	return ret.ret0;
}

int64_t sdei_event_register(int ev, sdei_handler_t *ep,
	uint64_t ep_arg, int flags, uint64_t mpidr)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_REGISTER;
	args.arg1 = ev;
	args.arg2 = (u_register_t)ep;
	args.arg3 = ep_arg;
	args.arg4 = flags;
	args.arg5 = mpidr;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_unregister(int ev)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_UNREGISTER;
	args.arg1 = ev;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_enable(int ev)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_ENABLE;
	args.arg1 = ev;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_disable(int ev)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_DISABLE;
	args.arg1 = ev;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_pe_mask(void)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_PE_MASK;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_pe_unmask(void)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_PE_UNMASK;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_private_reset(void)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_PRIVATE_RESET;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_shared_reset(void)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_SHARED_RESET;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_signal(uint64_t mpidr)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_SIGNAL;
	args.arg1 = 0; /* must be event 0 */
	args.arg2 = mpidr;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_status(int32_t ev)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_STATUS;
	args.arg1 = ev;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_routing_set(int32_t ev, uint64_t flags)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_ROUTING_SET;
	args.arg1 = ev;
	args.arg2 = flags;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_context(uint32_t param)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_CONTEXT;
	args.arg1 = param;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_complete(uint32_t flags)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_COMPLETE;
	args.arg1 = flags;
	ret = tftf_smc(&args);
	return ret.ret0;
}

int64_t sdei_event_complete_and_resume(uint64_t addr)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = SDEI_EVENT_COMPLETE_AND_RESUME;
	args.arg1 = addr;
	ret = tftf_smc(&args);
	return ret.ret0;
}
