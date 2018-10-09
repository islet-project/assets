/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <amu.h>
#include <amu_private.h>
#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>

int amu_supported(void)
{
	uint64_t features;

	features = read_id_aa64pfr0_el1() >> ID_AA64PFR0_AMU_SHIFT;
	return (features & ID_AA64PFR0_AMU_MASK) == 1;
}

/* Read the group 0 counter identified by the given `idx`. */
uint64_t amu_group0_cnt_read(int idx)
{
	assert(amu_supported());
	assert(idx >= 0 && idx < AMU_GROUP0_NR_COUNTERS);

	return amu_group0_cnt_read_internal(idx);
}

/* Read the group 1 counter identified by the given `idx`. */
uint64_t amu_group1_cnt_read(int idx)
{
	assert(amu_supported());
	assert(idx >= 0 && idx < AMU_GROUP1_NR_COUNTERS);

	return amu_group1_cnt_read_internal(idx);
}
