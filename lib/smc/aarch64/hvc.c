/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdint.h>
#include <tftf.h>

hvc_ret_values asm_tftf_hvc64(uint32_t fid,
			      u_register_t arg1,
			      u_register_t arg2,
			      u_register_t arg3,
			      u_register_t arg4,
			      u_register_t arg5,
			      u_register_t arg6,
			      u_register_t arg7);

hvc_ret_values tftf_hvc(const hvc_args *args)
{
	return asm_tftf_hvc64(args->fid,
			      args->arg1,
			      args->arg2,
			      args->arg3,
			      args->arg4,
			      args->arg5,
			      args->arg6,
			      args->arg7);
}
