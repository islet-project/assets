/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Definitions related to the True Random Number Generator (TRNG)
 * as per the SMC Calling Convention.
 *
 * TRNG calls are a subset of the Standard Service Calls.
 */

#ifndef __TRNG_H__
#define __TRNG_H__

#ifndef __ASSEMBLY__
#include <platform_def.h>
#include <stdbool.h>
#include <stdint.h>
#include <tftf_lib.h>
#endif

/*******************************************************************************
 * Macro to create the array entry for trng_functions[]
 ******************************************************************************/
#define DEFINE_TRNG_FUNC(_func_id, _mandatory) \
	{ SMC_##_func_id, _mandatory, "SMC_" # _func_id }

/*******************************************************************************
 * Defines for runtime services function ids
 ******************************************************************************/
#define SMC_TRNG_VERSION			0x84000050
#define SMC_TRNG_FEATURES			0x84000051
#define SMC_TRNG_UUID				0x84000052

#ifdef __aarch64__
#define SMC_TRNG_RND				0xc4000053
#define TRNG_MAX_BITS				U(192)
#define TRNG_ENTROPY_MASK			U(0xFFFFFFFFFFFFFFFF)
#else
#define SMC_TRNG_RND				0x84000053
#define TRNG_MAX_BITS				U(96)
#define TRNG_ENTROPY_MASK			U(0xFFFFFFFF)
#endif  /* __aarch64__ */

/*
 * Number of TRNG calls defined in the TRNG specification.
 */
#define TRNG_NUM_CALLS				(4U)

#ifndef __ASSEMBLY__
typedef struct {
	uint32_t	id;
	bool		mandatory;
	const char	*str;
} trng_function_t;

extern const trng_function_t trng_functions[TRNG_NUM_CALLS];
int32_t tftf_trng_version(void);
bool tftf_trng_feature_implemented(uint32_t id);
smc_ret_values tftf_trng_uuid(void);
smc_ret_values tftf_trng_rnd(uint32_t nbits);
#endif /* __ASSEMBLY__ */


/*******************************************************************************
 * TRNG version
 ******************************************************************************/
#define TRNG_MAJOR_VER_SHIFT	        (16)
#define TRNG_VERSION(major, minor)	((major << TRNG_MAJOR_VER_SHIFT)| minor)

/*******************************************************************************
 * TRNG error codes
 ******************************************************************************/
#define TRNG_E_SUCCESS			(0)
#define TRNG_E_NOT_SUPPORTED		(-1)
#define TRNG_E_INVALID_PARAMS		(-2)
#define TRNG_E_NO_ENTROPY		(-3)
#define TRNG_E_NOT_IMPLEMENTED		(-4)

#endif /* __TRNG_H__ */
