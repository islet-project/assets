/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>
#include <ffa_helpers.h>
#include <test_helpers.h>

#define SENDER HYP_ID
#define RECEIVER SP_ID(1)

static const struct ffa_uuid expected_sp_uuids[] = { {PRIMARY_UUID} };

static test_result_t simd_vector_compare(simd_vector_t a[SIMD_NUM_VECTORS],
					 simd_vector_t b[SIMD_NUM_VECTORS])
{
	for (unsigned int num = 0U; num < SIMD_NUM_VECTORS; num++) {
		if (memcmp(a[num], b[num], sizeof(simd_vector_t)) != 0) {
			ERROR("Vectors not equal: a:0x%llx b:0x%llx\n",
				(uint64_t)a[num][0], (uint64_t)b[num][0]);
			return TEST_RESULT_FAIL;
		}
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * Tests that SIMD vectors are preserved during the context switches between
 * normal world and the secure world.
 * Fills the SIMD vectors with known values, requests SP to fill the vectors
 * with a different values, checks that the context is restored on return.
 */
test_result_t test_simd_vectors_preserved(void)
{
	SKIP_TEST_IF_AARCH32();

	/**********************************************************************
	 * Verify that FFA is there and that it has the correct version.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	simd_vector_t simd_vectors_send[SIMD_NUM_VECTORS],
		      simd_vectors_receive[SIMD_NUM_VECTORS];

	/* 0x11 is just a dummy value to be distinguished from the value in the
	 * secure world. */
	for (unsigned int num = 0U; num < SIMD_NUM_VECTORS; num++) {
		memset(simd_vectors_send[num], 0x11 * num, sizeof(simd_vector_t));
	}

	fill_simd_vector_regs(simd_vectors_send);

	smc_ret_values ret = cactus_req_simd_fill_send_cmd(SENDER, RECEIVER);

	if (!is_ffa_direct_response(ret)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) == CACTUS_ERROR) {
		return TEST_RESULT_FAIL;
	}

	read_simd_vector_regs(simd_vectors_receive);

	return simd_vector_compare(simd_vectors_send, simd_vectors_receive);
}
