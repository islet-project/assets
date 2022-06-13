/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <smccc.h>
#include <tftf_lib.h>

#define RMI_FNUM_MIN_VALUE	U(0x150)
#define RMI_FNUM_MAX_VALUE	U(0x18F)

/* Get RMI fastcall std FID from offset */
#define SMC64_RMI_FID(_offset)					  \
	((SMC_TYPE_FAST << FUNCID_TYPE_SHIFT)			| \
	 (SMC_64 << FUNCID_CC_SHIFT)				| \
	 (OEN_STD_START << FUNCID_OEN_SHIFT)			| \
	 (((RMI_FNUM_MIN_VALUE + (_offset)) & FUNCID_NUM_MASK)	  \
	  << FUNCID_NUM_SHIFT))

/* RMI SMC64 FIDs handled by the RMMD */
#define RMI_RMM_REQ_VERSION		SMC64_RMI_FID(U(0))
#define SMC_RMM_GRANULE_DELEGATE	SMC64_RMI_FID(U(1))
#define SMC_RMM_GRANULE_UNDELEGATE	SMC64_RMI_FID(U(2))
#define SMC_RMM_REALM_CREATE		SMC64_RMI_FID(U(8))
#define SMC_RMM_REALM_DESTROY		SMC64_RMI_FID(U(9))

#define RMI_ABI_VERSION_GET_MAJOR(_version) ((_version) >> 16)
#define RMI_ABI_VERSION_GET_MINOR(_version) ((_version) & 0xFFFF)

#define NUM_GRANULES			5
#define NUM_RANDOM_ITERATIONS		7
#define GRANULE_SIZE			4096

#define B_DELEGATED			0
#define B_UNDELEGATED			1

#define NUM_CPU_DED_SPM			PLATFORM_CORE_COUNT / 2
/*
 * The error code 513 is the packed version of the
 * rmm error {RMM_STATUS_ERROR_INPUT,2}
 * happened when Granule(params_ptr).pas != NS
 */
#define RMM_STATUS_ERROR_INPUT		513UL
u_register_t realm_version(void);
u_register_t realm_granule_delegate(uintptr_t);
u_register_t realm_granule_undelegate(uintptr_t);
u_register_t realm_create(uintptr_t, uintptr_t);
u_register_t realm_destroy(uintptr_t);