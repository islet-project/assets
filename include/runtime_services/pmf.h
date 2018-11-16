/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PMF_H__
#define __PMF_H__

/*
 * Constants used for/by PMF services.
 */
#define PMF_ARM_TIF_IMPL_ID	0x41
#define PMF_TID_SHIFT		0
#define PMF_TID_MASK		(0xFF << PMF_TID_SHIFT)
#define PMF_SVC_ID_SHIFT	10
#define PMF_SVC_ID_MASK		(0x3F << PMF_SVC_ID_SHIFT)
#define PMF_IMPL_ID_SHIFT	24
#define PMF_IMPL_ID_MASK	(0xFFU << PMF_IMPL_ID_SHIFT)

/*
 * Flags passed to PMF_GET_TIMESTAMP_XXX and PMF_CAPTURE_TIMESTAMP
 */
#define PMF_CACHE_MAINT		(1 << 0)
#define PMF_NO_CACHE_MAINT	0

/*
 * Defines for PMF SMC function ids.
 */
#ifdef AARCH32
#define PMF_SMC_GET_TIMESTAMP	0x82000010
#else
#define PMF_SMC_GET_TIMESTAMP	0xC2000010
#endif

#define PMF_NUM_SMC_CALLS		2

/* Following are the supported PMF service IDs */
#define PMF_PSCI_STAT_SVC_ID	0
#define PMF_RT_INSTR_SVC_ID	1

#endif /* __PMF_H__ */
