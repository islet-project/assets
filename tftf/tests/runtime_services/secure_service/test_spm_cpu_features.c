/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>
#include <ffa_helpers.h>
#include <test_helpers.h>

#define SENDER HYP_ID
#define RECEIVER SP_ID(1)
#define SVE_TEST_ITERATIONS	100
#define SVE_ARRAYSIZE		1024

static const struct ffa_uuid expected_sp_uuids[] = { {PRIMARY_UUID} };

extern void sve_subtract_interleaved_smc(int *difference, const int *sve_op_1,
				       const int *sve_op_2);

static test_result_t fp_vector_compare(uint8_t *a, uint8_t *b,
	size_t vector_size, uint8_t vectors_num)
{
	if (memcmp(a, b, vector_size * vectors_num) != 0) {
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}

#ifdef __aarch64__
static sve_vector_t sve_vectors_input[SVE_NUM_VECTORS] __aligned(16);
static sve_vector_t sve_vectors_output[SVE_NUM_VECTORS] __aligned(16);
static int sve_op_1[SVE_ARRAYSIZE];
static int sve_op_2[SVE_ARRAYSIZE];
#endif

/*
 * Tests that SIMD vectors are preserved during the context switches between
 * normal world and the secure world.
 * Fills the SIMD vectors with known values, requests SP to fill the vectors
 * with a different values, checks that the context is restored on return.
 */
test_result_t test_simd_vectors_preserved(void)
{
	/**********************************************************************
	 * Verify that FF-A is there and that it has the correct version.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	simd_vector_t simd_vectors_send[SIMD_NUM_VECTORS],
		      simd_vectors_receive[SIMD_NUM_VECTORS];

	/* 0x11 is just a dummy value to be distinguished from the value in the
	 * secure world. */
	for (unsigned int num = 0U; num < SIMD_NUM_VECTORS; num++) {
		memset(simd_vectors_send[num], 0x11 * (num+1), sizeof(simd_vector_t));
	}
	fill_simd_vector_regs(simd_vectors_send);

	struct ffa_value ret = cactus_req_simd_fill_send_cmd(SENDER, RECEIVER);

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
#ifdef __aarch64__
	uint64_t vl;
	uint8_t *sve_vector;

	SKIP_TEST_IF_SVE_NOT_SUPPORTED();

	/**********************************************************************
	 * Verify that FF-A is there and that it has the correct version.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	/*
	 * Clear SVE vectors buffers used to compare the SVE state before calling
	 * into the Swd compared to SVE state restored after returning to NWd.
	 */
	memset(sve_vectors_input, sizeof(sve_vector_t) * SVE_NUM_VECTORS, 0);
	memset(sve_vectors_output, sizeof(sve_vector_t) * SVE_NUM_VECTORS, 0);

	/* Set ZCR_EL2.LEN to implemented VL (constrained by EL3). */
	write_zcr_el2(0xf);
	isb();

	/* Get the implemented VL. */
	vl = sve_vector_length_get();

	/* Fill each vector for the VL size with a fixed pattern. */
	sve_vector = (uint8_t *) sve_vectors_input;
	for (uint32_t vector_num = 0U; vector_num < SVE_NUM_VECTORS; vector_num++) {
		memset(sve_vector, 0x11 * (vector_num + 1), vl);
		sve_vector += vl;
	}

	/* Fill SVE vector registers with the buffer contents prepared above. */
	fill_sve_vector_regs(sve_vectors_input);

	/*
	 * Call cactus secure partition which uses SIMD (and expect it doesn't
	 * affect the normal world state on return).
	 */
	struct ffa_value ret = cactus_req_simd_fill_send_cmd(SENDER, RECEIVER);

	if (!is_ffa_direct_response(ret)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) == CACTUS_ERROR) {
		return TEST_RESULT_FAIL;
	}

	/* Get the SVE vectors state after returning to normal world. */
	read_sve_vector_regs(sve_vectors_output);

	/* Compare to state before calling into secure world. */
	return fp_vector_compare((uint8_t *)sve_vectors_input,
				 (uint8_t *)sve_vectors_output,
				 vl, SVE_NUM_VECTORS);
#else
	return TEST_RESULT_SKIPPED;
#endif /* __aarch64__ */
}

/*
 * Tests that SVE vector operations in normal world are not affected by context
 * switches between normal world and the secure world.
 */
test_result_t test_sve_vectors_operations(void)
{
#ifdef __aarch64__
	unsigned int val;

	SKIP_TEST_IF_SVE_NOT_SUPPORTED();

	/**********************************************************************
	 * Verify that FF-A is there and that it has the correct version.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	val = 2 * SVE_TEST_ITERATIONS;

	for (unsigned int i = 0; i < SVE_ARRAYSIZE; i++) {
		sve_op_1[i] = val;
		sve_op_2[i] = 1;
	}

	/* Set ZCR_EL2.LEN to implemented VL (constrained by EL3). */
	write_zcr_el2(0xf);
	isb();

	for (unsigned int i = 0; i < SVE_TEST_ITERATIONS; i++) {
		/* Perform SVE operations with intermittent calls to Swd. */
		sve_subtract_interleaved_smc(sve_op_1, sve_op_1, sve_op_2);
	}

	/* Check result of SVE operations. */
	for (unsigned int i = 0; i < SVE_ARRAYSIZE; i++) {
		if (sve_op_1[i] != (val - SVE_TEST_ITERATIONS)) {
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
#else
	return TEST_RESULT_SKIPPED;
#endif /* __aarch64__ */
}
