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

static test_result_t fp_vector_compare(uint8_t *a, uint8_t *b,
	size_t vector_size, uint8_t vectors_num)
{
	if (memcmp(a, b, vector_size * vectors_num) != 0) {
		return TEST_RESULT_FAIL;
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

	return fp_vector_compare((uint8_t *)simd_vectors_send,
				 (uint8_t *)simd_vectors_receive,
				 sizeof(simd_vector_t), SIMD_NUM_VECTORS);
}

/*
 * Tests that SVE vectors are preserved during the context switches between
 * normal world and the secure world.
 * Fills the SVE vectors with known values, requests SP to fill the vectors
 * with a different values, checks that the context is restored on return.
 */
test_result_t test_sve_vectors_preserved(void)
{
	SKIP_TEST_IF_SVE_NOT_SUPPORTED();

	/**********************************************************************
	 * Verify that FFA is there and that it has the correct version.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	sve_vector_t sve_vectors_send[SVE_NUM_VECTORS],
		     sve_vectors_receive[SVE_NUM_VECTORS];

	/* 0x11 is just a dummy value to be distinguished from the value in the
	 * secure world. */
	for (unsigned int num = 0U; num < SVE_NUM_VECTORS; num++) {
		memset(sve_vectors_send[num], 0x11 * num, sizeof(sve_vector_t));
	}

	fill_sve_vector_regs(sve_vectors_send);

	smc_ret_values ret = cactus_req_simd_fill_send_cmd(SENDER, RECEIVER);

	if (!is_ffa_direct_response(ret)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) == CACTUS_ERROR) {
		return TEST_RESULT_FAIL;
	}

	read_sve_vector_regs(sve_vectors_receive);

	return fp_vector_compare((uint8_t *)sve_vectors_send,
				 (uint8_t *)sve_vectors_receive,
				 sizeof(sve_vector_t), SVE_NUM_VECTORS);
}
