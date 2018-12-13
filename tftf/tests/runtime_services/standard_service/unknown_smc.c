/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <smccc.h>
#include <std_svc.h>
#include <tftf_lib.h>
#include <uuid_utils.h>

/* Invalid SMC FID: chose an identifier that falls in the reserved space */
#define INVALID_FID     (0x00ff0000 | (1u << 31))

/**
 * @Test_Aim@ Force an SMC_UNKNOWN return
 */
test_result_t test_unknown_smc(void)
{
	smc_args unk_smc;
	smc_ret_values ret;

	unk_smc.fid = INVALID_FID;
	ret = tftf_smc(&unk_smc);

	if (ret.ret0 != SMC_UNKNOWN) {
		tftf_testcase_printf("Expected SMC_UNKNOWN, got %ld\n",
				(long int) ret.ret0);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
