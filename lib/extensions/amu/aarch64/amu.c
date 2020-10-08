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
 * Get AMU version value from aa64pfr0.
 * Return values
 *   ID_AA64PFR0_AMU_V1: FEAT_AMUv1 supported (introduced in ARM v8.4)
 *   ID_AA64PFR0_AMU_V1P1: FEAT_AMUv1p1 supported (introduced in ARM v8.6)
 *   ID_AA64PFR0_AMU_NOT_SUPPORTED: not supported
 */
unsigned int amu_get_version(void)
{
	return (unsigned int)(read_id_aa64pfr0_el1() >> ID_AA64PFR0_AMU_SHIFT) &
		ID_AA64PFR0_AMU_MASK;
}

/* Check if group 1 counters is implemented */
int amu_group1_supported(void)
{
	uint64_t features = read_amcfgr_el0() >> AMCFGR_EL0_NCG_SHIFT;

	return (features & AMCFGR_EL0_NCG_MASK) == 1U;
}

/* Read the group 0 counter identified by the given `idx`. */
uint64_t amu_group0_cnt_read(unsigned int idx)
{
	assert(amu_get_version() != ID_AA64PFR0_AMU_NOT_SUPPORTED);
	assert(idx < AMU_GROUP0_NR_COUNTERS);

	return amu_group0_cnt_read_internal(idx);
}

/*
 * Read the group 0 offset register for a given index. Index must be 0, 2, or
 * 3, the register for 1 does not exist.
 *
 * Using this function requires v8.6 FEAT_AMUv1p1 support.
 */
uint64_t amu_group0_voffset_read(unsigned int idx)
{
	assert(amu_get_version() >= ID_AA64PFR0_AMU_V1P1);
	assert(idx < AMU_GROUP0_NR_COUNTERS);
	assert(idx != 1U);

	return amu_group0_voffset_read_internal(idx);
}

/*
 * Write the group 0 offset register for a given index. Index must be 0, 2, or
 * 3, the register for 1 does not exist.
 *
 * Using this function requires v8.6 FEAT_AMUv1p1 support.
 */
void amu_group0_voffset_write(unsigned int idx, uint64_t val)
{
	assert(amu_get_version() >= ID_AA64PFR0_AMU_V1P1);
	assert(idx < AMU_GROUP0_NR_COUNTERS);
	assert(idx != 1U);

	amu_group0_voffset_write_internal(idx, val);
	isb();
}

/* Read the group 1 counter identified by the given `idx`. */
uint64_t amu_group1_cnt_read(unsigned int idx)
{
	assert(amu_get_version() != ID_AA64PFR0_AMU_NOT_SUPPORTED);
	assert(idx < AMU_GROUP1_NR_COUNTERS);

	return amu_group1_cnt_read_internal(idx);
}

/*
 * Read the group 1 offset register for a given index.
 *
 * Using this function requires v8.6 FEAT_AMUv1p1 support.
 */
uint64_t amu_group1_voffset_read(unsigned int idx)
{
	assert(amu_get_version() >= ID_AA64PFR0_AMU_V1P1);
	assert(amu_group1_supported());
	assert(idx < AMU_GROUP1_NR_COUNTERS);
	assert(((read_amcg1idr_el0() >> AMCG1IDR_VOFF_SHIFT) &
		(1U << idx)) != 0U);

	return amu_group1_voffset_read_internal(idx);
}

/*
 * Write the group 1 offset register for a given index.
 *
 * Using this function requires v8.6 FEAT_AMUv1p1 support.
 */
void amu_group1_voffset_write(unsigned int idx, uint64_t val)
{
	assert(amu_get_version() >= ID_AA64PFR0_AMU_V1P1);
	assert(amu_group1_supported());
	assert(idx < AMU_GROUP1_NR_COUNTERS);
	assert(((read_amcg1idr_el0() >> AMCG1IDR_VOFF_SHIFT) &
		(1U << idx)) != 0U);

	amu_group1_voffset_write_internal(idx, val);
	isb();
}
