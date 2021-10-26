/*
 * Copyright (c) 2019-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_FEATURES_H
#define ARCH_FEATURES_H

#include <stdbool.h>

#include <arch_helpers.h>

static inline bool is_armv7_gentimer_present(void)
{
	return ((read_id_pfr1() >> ID_PFR1_GENTIMER_SHIFT) &
		ID_PFR1_GENTIMER_MASK) != 0U;
}

static inline bool is_armv8_2_sve_present(void)
{
	/* SVE is not usable in aarch32 */
	return false;
}

static inline bool is_armv8_2_ttcnp_present(void)
{
	return ((read_id_mmfr4() >> ID_MMFR4_CNP_SHIFT) &
		ID_MMFR4_CNP_MASK) != 0U;
}

static inline uint32_t arch_get_debug_version(void)
{
	return ((read_dbgdidr() & DBGDIDR_VERSION_BITS) >>
		DBGDIDR_VERSION_SHIFT);
}

static inline bool get_armv8_4_trf_support(void)
{
	return ((read_id_dfr0() >> ID_DFR0_TRACEFILT_SHIFT) &
		ID_DFR0_TRACEFILT_MASK) ==
		ID_DFR0_TRACEFILT_SUPPORTED;
}

static inline bool is_armv8_4_dit_present(void)
{
	return ((read_id_pfr0() >> ID_PFR0_DIT_SHIFT) &
		ID_PFR0_DIT_MASK) != 0;
}

static inline bool get_armv8_0_sys_reg_trace_support(void)
{
	return ((read_id_dfr0() >> ID_DFR0_COPTRC_SHIFT) &
		ID_DFR0_COPTRC_MASK) ==
		ID_DFR0_COPTRC_SUPPORTED;
}

static inline unsigned int get_armv9_2_feat_rme_support(void)
{
	return 0;
}
#endif /* ARCH_FEATURES_H */
