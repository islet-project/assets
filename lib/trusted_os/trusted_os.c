/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <smccc.h>
#include <stdint.h>
#include <tftf.h>
#include <trusted_os.h>
#include <uuid_utils.h>

unsigned int is_trusted_os_present(uuid_t *tos_uuid)
{
	smc_args tos_uid_args = { SMC_TOS_UID };
	smc_ret_values ret;

	ret = tftf_smc(&tos_uid_args);

	if ((ret.ret0 == SMC_UNKNOWN) ||
	    ((ret.ret0 == 0) && (ret.ret1 == 0) && (ret.ret2 == 0) &&
	     (ret.ret3 == 0)))
		return 0;

	if (tos_uuid != NULL) {
		uint32_t *tos_uuid32 = (uint32_t *) tos_uuid;
		tos_uuid32[0] = ret.ret0;
		tos_uuid32[1] = ret.ret1;
		tos_uuid32[2] = ret.ret2;
		tos_uuid32[3] = ret.ret3;
	}

	return 1;
}
