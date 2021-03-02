/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <amu.h>
#include <amu_private.h>
#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>

/*
 * Get AMU version value from pfr0.
 * Return values
 *   ID_PFR0_AMU_V1: FEAT_AMUv1 supported (introduced in ARM v8.4)
 *   ID_PFR0_AMU_V1P1: FEAT_AMUv1p1 supported (introduced in ARM v8.6)
 *   ID_PFR0_AMU_NOT_SUPPORTED: not supported
 */
unsigned int amu_get_version(void)
{
	return (unsigned int)(read_id_pfr0() >> ID_PFR0_AMU_SHIFT) &
		ID_PFR0_AMU_MASK;
}

/* Read the group 0 counter identified by the given `idx`. */
uint64_t amu_group0_cnt_read(unsigned int idx)
{
	assert(amu_get_version() != ID_PFR0_AMU_NOT_SUPPORTED);
	assert(idx < AMU_GROUP0_NR_COUNTERS);

	return amu_group0_cnt_read_internal(idx);
}

/* Read the group 1 counter identified by the given `idx`. */
uint64_t amu_group1_cnt_read(unsigned int idx)
{
	assert(amu_get_version() != ID_PFR0_AMU_NOT_SUPPORTED);
	assert(idx < AMU_GROUP1_NR_COUNTERS);

	return amu_group1_cnt_read_internal(idx);
}
