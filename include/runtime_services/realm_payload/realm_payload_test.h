/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <smccc.h>
#include <tftf_lib.h>

#define RMI_FNUM_MIN_VALUE	U(0x150)
#define RMI_FNUM_MAX_VALUE	U(0x18F)

/* Get RMI fastcall std FID from function number */
#define RMI_FID(smc_cc, func_num)			\
	((SMC_TYPE_FAST << FUNCID_TYPE_SHIFT)	|	\
	((smc_cc) << FUNCID_CC_SHIFT)		|	\
	(OEN_STD_START << FUNCID_OEN_SHIFT)	|	\
	((func_num) << FUNCID_NUM_SHIFT))

/*
 * SMC_RMM_INIT_COMPLETE is the only function in the RMI that originates from
 * the Realm world and is handled by the RMMD. The remaining functions are
 * always invoked by the Normal world, forwarded by RMMD and handled by the
 * RMM
 */
#define RMI_FNUM_VERSION_REQ		U(0x150)
#define RMI_FNUM_GRANULE_DELEGATE	U(0x151)
#define RMI_FNUM_GRANULE_UNDELEGATE	U(0x152)
#define RMI_FNUM_REALM_CREATE		U(0x158)
#define RMI_FNUM_REALM_DESTROY		U(0x159)

/********************************************************************************/


/* RMI SMC64 FIDs handled by the RMMD */
#define RMI_RMM_REQ_VERSION		RMI_FID(SMC_64, RMI_FNUM_VERSION_REQ)

#define SMC_RMM_GRANULE_DELEGATE	RMI_FID(SMC_64, \
						RMI_FNUM_GRANULE_DELEGATE)
#define SMC_RMM_GRANULE_UNDELEGATE	RMI_FID(SMC_64, \
						RMI_FNUM_GRANULE_UNDELEGATE)
#define SMC_RMM_REALM_CREATE		RMI_FID(SMC_64, \
						RMI_FNUM_REALM_CREATE)
#define SMC_RMM_REALM_DESTROY		RMI_FID(SMC_64, \
						RMI_FNUM_REALM_DESTROY)

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