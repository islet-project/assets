/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Definitions related to the Power State Coordination Interface (PSCI)
 * as per the SMC Calling Convention.
 *
 * PSCI calls are a subset of the Standard Service Calls.
 */

#ifndef __PSCI_H__
#define __PSCI_H__

#ifndef __ASSEMBLY__
#include <assert.h>
#include <platform_def.h>
#include <stdbool.h>
#include <stdint.h>
#endif

/*******************************************************************************
 * Macro to create the array entry for psci_functions[]
 ******************************************************************************/
#define DEFINE_PSCI_FUNC(_func_id, _mandatory) \
	{ SMC_##_func_id, _mandatory, "SMC_" # _func_id }

/*******************************************************************************
 * Defines for runtime services function ids
 ******************************************************************************/
#define SMC_PSCI_VERSION			0x84000000
#define SMC_PSCI_CPU_SUSPEND_AARCH32		0x84000001
#define SMC_PSCI_CPU_SUSPEND_AARCH64		0xc4000001
#define SMC_PSCI_CPU_OFF			0x84000002
#define SMC_PSCI_CPU_ON_AARCH32			0x84000003
#define SMC_PSCI_CPU_ON_AARCH64			0xc4000003
#define SMC_PSCI_AFFINITY_INFO_AARCH32		0x84000004
#define SMC_PSCI_AFFINITY_INFO_AARCH64		0xc4000004
#define SMC_PSCI_MIG_AARCH32			0x84000005
#define SMC_PSCI_MIG_AARCH64			0xc4000005
#define SMC_PSCI_MIG_INFO_TYPE			0x84000006
#define SMC_PSCI_MIG_INFO_UP_CPU_AARCH32	0x84000007
#define SMC_PSCI_MIG_INFO_UP_CPU_AARCH64	0xc4000007
#define SMC_PSCI_SYSTEM_OFF			0x84000008
#define SMC_PSCI_SYSTEM_RESET			0x84000009
#define SMC_PSCI_FEATURES			0x8400000a
#define SMC_PSCI_CPU_FREEZE			0x8400000b
#define SMC_PSCI_CPU_DEFAULT_SUSPEND32		0x8400000c
#define SMC_PSCI_CPU_DEFAULT_SUSPEND64		0xc400000c
#define SMC_PSCI_CPU_HW_STATE32			0x8400000d
#define SMC_PSCI_CPU_HW_STATE64			0xc400000d
#define SMC_PSCI_SYSTEM_SUSPEND32		0x8400000e
#define SMC_PSCI_SYSTEM_SUSPEND64		0xc400000e
#define SMC_PSCI_SET_SUSPEND_MODE		0x8400000f
#define SMC_PSCI_STAT_RESIDENCY32		0x84000010
#define SMC_PSCI_STAT_RESIDENCY64		0xc4000010
#define SMC_PSCI_STAT_COUNT32			0x84000011
#define SMC_PSCI_STAT_COUNT64			0xc4000011
#define SMC_PSCI_RESET2_AARCH32			0x84000012
#define SMC_PSCI_RESET2_AARCH64			0xc4000012
#define SMC_PSCI_MEM_PROTECT			0x84000013
#define SMC_PSCI_MEM_PROTECT_CHECK_RANGE32	0x84000014
#define SMC_PSCI_MEM_PROTECT_CHECK_RANGE64	0xc4000014

/*
 * Architecture-specific SMC function IDs
 */
#ifndef AARCH32
#define SMC_PSCI_CPU_SUSPEND		SMC_PSCI_CPU_SUSPEND_AARCH64
#define SMC_PSCI_CPU_ON			SMC_PSCI_CPU_ON_AARCH64
#define SMC_PSCI_AFFINITY_INFO		SMC_PSCI_AFFINITY_INFO_AARCH64
#define SMC_PSCI_MIG			SMC_PSCI_MIG_AARCH64
#define SMC_PSCI_MIG_INFO_UP_CPU	SMC_PSCI_MIG_INFO_UP_CPU_AARCH64
#define SMC_PSCI_CPU_DEFAULT_SUSPEND	SMC_PSCI_CPU_DEFAULT_SUSPEND64
#define SMC_PSCI_CPU_HW_STATE		SMC_PSCI_CPU_HW_STATE64
#define SMC_PSCI_SYSTEM_SUSPEND		SMC_PSCI_SYSTEM_SUSPEND64
#define SMC_PSCI_STAT_RESIDENCY		SMC_PSCI_STAT_RESIDENCY64
#define SMC_PSCI_STAT_COUNT		SMC_PSCI_STAT_COUNT64
#define SMC_PSCI_RESET2			SMC_PSCI_RESET2_AARCH64
#define SMC_PSCI_MEM_PROTECT_CHECK	SMC_PSCI_MEM_PROTECT_CHECK_RANGE64
#else
#define SMC_PSCI_CPU_SUSPEND		SMC_PSCI_CPU_SUSPEND_AARCH32
#define SMC_PSCI_CPU_ON			SMC_PSCI_CPU_ON_AARCH32
#define SMC_PSCI_AFFINITY_INFO		SMC_PSCI_AFFINITY_INFO_AARCH32
#define SMC_PSCI_MIG			SMC_PSCI_MIG_AARCH32
#define SMC_PSCI_MIG_INFO_UP_CPU	SMC_PSCI_MIG_INFO_UP_CPU_AARCH32
#define SMC_PSCI_CPU_DEFAULT_SUSPEND	SMC_PSCI_CPU_DEFAULT_SUSPEND32
#define SMC_PSCI_CPU_HW_STATE		SMC_PSCI_CPU_HW_STATE32
#define SMC_PSCI_SYSTEM_SUSPEND		SMC_PSCI_SYSTEM_SUSPEND32
#define SMC_PSCI_STAT_RESIDENCY		SMC_PSCI_STAT_RESIDENCY32
#define SMC_PSCI_STAT_COUNT		SMC_PSCI_STAT_COUNT32
#define SMC_PSCI_RESET2			SMC_PSCI_RESET2_AARCH32
#define SMC_PSCI_MEM_PROTECT_CHECK	SMC_PSCI_MEM_PROTECT_CHECK_RANGE32
#endif

/*
 * Number of PSCI calls defined in the PSCI specification.
 */
#define PSCI_NUM_CALLS				33

#ifndef __ASSEMBLY__
typedef struct {
	uint32_t	id;
	bool		mandatory;
	const char	*str;
} psci_function_t;

extern const psci_function_t psci_functions[PSCI_NUM_CALLS];
#endif /* __ASSEMBLY__ */

/*******************************************************************************
 * PSCI Migrate specific defines
 ******************************************************************************/
#define PSCI_TOS_UP_MIG_CAP	0
#define PSCI_TOS_NOT_UP_MIG_CAP	1
#define PSCI_TOS_NOT_PRESENT_MP	2

/*******************************************************************************
 * PSCI CPU_SUSPEND 'power_state' parameter specific defines
 ******************************************************************************/
/* Original format */
#define PSTATE_ID_SHIFT		0
#define PSTATE_TYPE_SHIFT	16
#define PSTATE_AFF_LVL_SHIFT	24
#define PSTATE_ID_MASK		0xFFFF
#define PSTATE_AFF_LVL_MASK	0x3

#define psci_state_id_valid(state_id)			\
	(((state_id) & ~PSTATE_ID_MASK) == 0)

#define psci_affinity_level_valid(aff_level)		\
	(((aff_level) & ~PSTATE_AFF_LVL_MASK) == 0)

/* Extended format */
#define PSTATE_ID_SHIFT_EXT	0
#define PSTATE_TYPE_SHIFT_EXT	30
#define PSTATE_ID_MASK_EXT	0xFFFFFFF

#define psci_state_id_ext_valid(state_id)		\
	(((state_id) & ~PSTATE_ID_MASK_EXT) == 0)

/* Definitions common to original and extended StateID formats */
#define PSTATE_TYPE_MASK	0x1

#define PSTATE_TYPE_STANDBY	0x0
#define PSTATE_TYPE_POWERDOWN	0x1

#define PSTATE_AFF_LVL_0	0
#define PSTATE_AFF_LVL_1	1
#define PSTATE_AFF_LVL_2	2
#define PSTATE_AFF_LVL_3	3

#define psci_state_type_valid(state_type)		\
	(((state_type) & ~PSTATE_TYPE_MASK) == 0)

/*******************************************************************************
 * PSCI 'Feature Flags' defines for CPU_SUSPEND
 ******************************************************************************/
#define CPU_SUSPEND_FEAT_OS_INIT_MODE_SHIFT	0
#define CPU_SUSPEND_FEAT_PSTATE_FORMAT_SHIFT	1
#define CPU_SUSPEND_FEAT_PSTATE_FORMAT_ORIGINAL	0
#define CPU_SUSPEND_FEAT_PSTATE_FORMAT_EXTENDED	1

#define CPU_SUSPEND_FEAT_VALID_MASK			\
	((1 << CPU_SUSPEND_FEAT_OS_INIT_MODE_SHIFT) |	\
	 (1 << CPU_SUSPEND_FEAT_PSTATE_FORMAT_SHIFT))

/*******************************************************************************
 * PSCI 'Feature Flags' defines for functions other than CPU_SUSPEND
 ******************************************************************************/
#define PSCI_FEATURE_FLAGS_ZERO			0

#ifndef __ASSEMBLY__

/*
 * Construct the local State-ID for a particular level according to
 * the platform specific local state-ID width.
 */
#define psci_make_local_state_id(level, local_state) \
		(((local_state) & ((1 << PLAT_LOCAL_PSTATE_WIDTH) - 1)) \
			<< (PLAT_LOCAL_PSTATE_WIDTH * (level)))
#endif

/*******************************************************************************
 * PSCI version
 ******************************************************************************/
#define PSCI_MAJOR_VER		1
#define PSCI_MINOR_VER		1
#define PSCI_MAJOR_VER_SHIFT	16
#define PSCI_MAJOR_VER_MASK	0xFFFF0000
#define PSCI_VERSION(major, minor)	((major << PSCI_MAJOR_VER_SHIFT) \
					| minor)

/*******************************************************************************
 * PSCI error codes
 ******************************************************************************/
#define PSCI_E_SUCCESS		0
#define PSCI_E_NOT_SUPPORTED	-1
#define PSCI_E_INVALID_PARAMS	-2
#define PSCI_E_DENIED		-3
#define PSCI_E_ALREADY_ON	-4
#define PSCI_E_ON_PENDING	-5
#define PSCI_E_INTERN_FAIL	-6
#define PSCI_E_NOT_PRESENT	-7
#define PSCI_E_DISABLED		-8
#define PSCI_E_INVALID_ADDRESS	-9

/*******************************************************************************
 * PSCI affinity state related constants.
 ******************************************************************************/
#define PSCI_STATE_ON		0x0
#define PSCI_STATE_OFF		0x1
#define PSCI_STATE_ON_PENDING	0x2

/*******************************************************************************
 * PSCI node hardware state related constants.
 ******************************************************************************/
#define PSCI_HW_STATE_ON	0x0
#define PSCI_HW_STATE_OFF	0x1
#define PSCI_HW_STATE_STANDBY	0x2

#endif /* __PSCI_H__ */
