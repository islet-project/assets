/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <assert.h>
#include <plat_topology.h>
#include <platform_def.h>
#include <stddef.h>
#include <tftf_lib.h>

static const struct {
	unsigned cluster_id;
	unsigned cpu_id;
} juno_cores[] = {
	/* Cortex-A53 Cluster: 4 cores*/
	{ 1, 0 },
	{ 1, 1 },
	{ 1, 2 },
	{ 1, 3 },
	/* Cortex-A57 Cluster: 2 cores */
	{ 0, 0 },
	{ 0, 1 },
};

/*
 * The Juno power domain tree descriptor. Juno implements a system
 * power domain at the level 2. The first entry in the power domain descriptor
 * specifies the number of power domains at the highest power level. For Juno
 * this is 1 i.e. the number of system power domain.
 */
static const unsigned char juno_power_domain_tree_desc[] = {
	/* Number of root nodes */
	PLATFORM_SYSTEM_COUNT,
	/* Number of children of root node */
	PLATFORM_CLUSTER_COUNT,
	/* Number of children for the first cluster */
	PLATFORM_CLUSTER1_CORE_COUNT,
	/* Number of children for the second cluster */
	PLATFORM_CLUSTER0_CORE_COUNT
};

const unsigned char *tftf_plat_get_pwr_domain_tree_desc(void)
{
	return juno_power_domain_tree_desc;
}

uint64_t tftf_plat_get_mpidr(unsigned int core_pos)
{
	assert(core_pos < PLATFORM_CORE_COUNT);

	return make_mpid(juno_cores[core_pos].cluster_id,
					juno_cores[core_pos].cpu_id);
}
