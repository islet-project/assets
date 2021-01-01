/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_test_cmds.h>
#include <debug.h>
#include <ffa_endpoints.h>
#include <smccc.h>
#include <test_helpers.h>

static const struct ffa_uuid expected_sp_uuids[] = { {PRIMARY_UUID} };

/**************************************************************************
 * Send a command to SP1 initiate DMA service with the help of a peripheral
 * device upstream of an SMMUv3 IP
 **************************************************************************/
test_result_t test_smmu_spm(void)
{
	smc_ret_values ret;

	/**********************************************************************
	 * Check SPMC has ffa_version and expected FFA endpoints are deployed.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	VERBOSE("Sending command to SP %x for initiating DMA transfer\n",
			SP_ID(1));
	ret = cactus_send_dma_cmd(HYP_ID, SP_ID(1));

	if (cactus_get_response(ret) != CACTUS_SUCCESS) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

