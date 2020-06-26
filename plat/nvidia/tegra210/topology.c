/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
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
	unsigned int cluster_id;
	unsigned int cpu_id;
} tegra210_cores[PLATFORM_CORE_COUNT] = {
	{ 0, 0 },
	{ 0, 1 },
	{ 0, 2 },
	{ 0, 3 }
};

/*
 * The Tegra210 power domain tree descriptor. Tegra186 implements a system
 * power domain at the level 2. The first entry in the power domain descriptor
 * specifies the number of power domains at the highest power level.
 */
static const unsigned char tegra210_power_domain_tree_desc[] = {
	/* Number of root nodes */
	PLATFORM_SYSTEM_COUNT,
	/* Number of children of root node */
	PLATFORM_CLUSTER_COUNT,
	/* Number of children for the cluster */
	PLATFORM_CORES_PER_CLUSTER
};

const unsigned char *tftf_plat_get_pwr_domain_tree_desc(void)
{
	return tegra210_power_domain_tree_desc;
}

uint64_t tftf_plat_get_mpidr(unsigned int core_pos)
{
	assert(core_pos < PLATFORM_CORE_COUNT);

	return (uint64_t)make_mpid(tegra210_cores[core_pos].cluster_id,
				tegra210_cores[core_pos].cpu_id);
}
