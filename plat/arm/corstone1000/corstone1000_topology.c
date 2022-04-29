/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stddef.h>
#include <arch.h>
#include <tftf_lib.h>
#include <plat_topology.h>
#include <platform_def.h>

static const struct {
	unsigned int cluster_id;
	unsigned int cpu_id;
} corstone1000_cores[] = {
	/* SMP with single core, cluster_id is same as cpu_id */
	{ 0, 0 },
};

/*
 * The Corstone1000 power domain tree descriptor. Corstone1000 implements a system
 * power domain at the level 2. The first entry in the power domain descriptor
 * specifies the number of power domains at the highest power level. For Corstone1000
 * this is 1 i.e. the number of system power domain.
 */
static const unsigned char corstone1000_power_domain_tree_desc[] = {
	/* Number of root nodes */
	PLATFORM_SYSTEM_COUNT,
	/* Number of children of root node */
	PLATFORM_CLUSTER_COUNT,
	/* Number of children for the second cluster */
	PLATFORM_CLUSTER0_CORE_COUNT
};

const unsigned char *tftf_plat_get_pwr_domain_tree_desc(void)
{
	return corstone1000_power_domain_tree_desc;
}

uint64_t tftf_plat_get_mpidr(unsigned int core_pos)
{
	assert(core_pos < PLATFORM_CORE_COUNT);

	return make_mpid(corstone1000_cores[core_pos].cluster_id,
					corstone1000_cores[core_pos].cpu_id);
}
