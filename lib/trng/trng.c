/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <irq.h>
#include <platform.h>
#include <trng.h>
#include <sgi.h>
#include <tftf.h>
#include <tftf_lib.h>

const trng_function_t trng_functions[TRNG_NUM_CALLS] = {
	DEFINE_TRNG_FUNC(TRNG_VERSION, true),
	DEFINE_TRNG_FUNC(TRNG_FEATURES, true),
	DEFINE_TRNG_FUNC(TRNG_UUID, true),
	DEFINE_TRNG_FUNC(TRNG_RND, true),
};

int32_t tftf_trng_version(void)
{
	smc_args args = { SMC_TRNG_VERSION };
	smc_ret_values ret_vals;

	ret_vals = tftf_smc(&args);
	return ret_vals.ret0;
}


bool tftf_trng_feature_implemented(uint32_t id)
{
	smc_args args = {
		SMC_TRNG_FEATURES,
		id,
	};
	smc_ret_values ret_vals;

	ret_vals = tftf_smc(&args);
	return ret_vals.ret0 == TRNG_E_SUCCESS;
}

smc_ret_values tftf_trng_uuid(void)
{
	smc_args args = { SMC_TRNG_UUID };

	return tftf_smc(&args);
}

smc_ret_values tftf_trng_rnd(uint32_t nbits)
{
	smc_args args = {
		SMC_TRNG_RND,
		nbits,
	};

	return tftf_smc(&args);
}
