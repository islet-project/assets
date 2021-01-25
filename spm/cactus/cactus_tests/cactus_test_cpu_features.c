/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cactus_test_cmds.h"
#include "spm_common.h"

/*
 * Fill SIMD vectors from secure world side with a unique value.
 * 0x22 is just a dummy value to be distinguished from the value
 * in the normal world.
 */
CACTUS_CMD_HANDLER(req_simd_fill, CACTUS_REQ_SIMD_FILL_CMD)
{
	simd_vector_t simd_vectors[SIMD_NUM_VECTORS];

	for (unsigned int num = 0U; num < SIMD_NUM_VECTORS; num++) {
		memset(simd_vectors[num], 0x22 * num, sizeof(simd_vector_t));
	}

	fill_simd_vector_regs(simd_vectors);

	return cactus_response(ffa_dir_msg_dest(*args),
			       ffa_dir_msg_source(*args),
			       CACTUS_SUCCESS);
}
