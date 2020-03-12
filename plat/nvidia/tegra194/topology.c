/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <assert.h>
#include <stddef.h>

#include <plat_topology.h>
#include <platform_def.h>
#include <tftf_lib.h>

static const struct {
	unsigned cluster_id;
	unsigned cpu_id;
} tegra194_cores[] = {
	{ 0, 0 },
	{ 0, 1 },
	{ 1, 0 },
	{ 1, 1 },
	{ 2, 0 },
	{ 2, 1 },
	{ 3, 0 },
	{ 3, 1 }
};

/*
 * The Tegra194 power domain tree descriptor. Tegra194 implements a system
 * power domain at the level 2. The first entry in the power domain descriptor
 * specifies the number of power domains at the highest power level.
 */
static const unsigned char tegra194_power_domain_tree_desc[] = {
	/* Number of root nodes */
	PLATFORM_SYSTEM_COUNT,
	/* Number of children of root node */
	PLATFORM_CLUSTER_COUNT,
	/* Number of children for the first cluster */
	PLATFORM_CORES_PER_CLUSTER,
	/* Number of children for the second cluster */
	PLATFORM_CORES_PER_CLUSTER,
	/* Number of children for the third cluster */
	PLATFORM_CORES_PER_CLUSTER,
	/* Number of children for the fourth cluster */
	PLATFORM_CORES_PER_CLUSTER
};

const unsigned char *tftf_plat_get_pwr_domain_tree_desc(void)
{
	return tegra194_power_domain_tree_desc;
}

uint64_t tftf_plat_get_mpidr(unsigned int core_pos)
{
	assert(core_pos < PLATFORM_CORE_COUNT);

	return make_mpid(tegra194_cores[core_pos].cluster_id,
			 tegra194_cores[core_pos].cpu_id);
}
