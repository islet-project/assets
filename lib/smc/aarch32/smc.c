/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <tftf.h>

void asm_tftf_smc32(const smc_args *args,
		smc_ret_values *smc_ret);

smc_ret_values tftf_smc(const smc_args *args)
{
	smc_ret_values ret = {0};
	asm_tftf_smc32(args, &ret);

	return ret;
}
