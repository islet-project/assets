/* Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ffa_helpers.h>
#include <test_helpers.h>
#include <tftf_lib.h>

test_result_t test_ffa_spm_id_get(void)
{
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 1);

	smc_ret_values ffa_ret = ffa_spm_id_get();

	if (is_ffa_call_error(ffa_ret)) {
		ERROR("FFA_SPM_ID_GET call failed! Error code: 0x%x\n",
			ffa_error_code(ffa_ret));
		return TEST_RESULT_FAIL;
	}

	/* Check the SPMC value given in the fvp_spmc_manifest is returned */
	ffa_id_t spm_id = ffa_endpoint_id(ffa_ret);

	if (spm_id != SPMC_ID) {
		ERROR("Expected SPMC_ID of 0x%x\n received: 0x%x\n",
			SPMC_ID, spm_id);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
