/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <plat_topology.h>
#include <tftf_lib.h>

static const struct {
	unsigned int cluster_id;
	unsigned int cpu_id;
} sgi575_cores[] = {
	/* Cluster0: 4 cores*/
	{ 0, 0 },
	{ 0, 1 },
	{ 0, 2 },
	{ 0, 3 },
	/* Cluster1: 4 cores */
	{ 1, 0 },
	{ 1, 1 },
	{ 1, 2 },
	{ 1, 3 },
};

/*
 * The power domain tree descriptor. The cluster power domains are
 * arranged so that when the PSCI generic code creates the power domain tree,
 * the indices of the CPU power domain nodes it allocates match the linear
 * indices returned by plat_core_pos_by_mpidr().
 */
const unsigned char sgi575_pd_tree_desc[] = {
	/* Number of root nodes */
	SGI_CLUSTER_COUNT,
	/* Number of children for the 1st node */
	SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 2nd node */
	SGI_MAX_CPUS_PER_CLUSTER
};

const unsigned char *tftf_plat_get_pwr_domain_tree_desc(void)
{
	return sgi575_pd_tree_desc;
}

uint64_t tftf_plat_get_mpidr(unsigned int core_pos)
{
	unsigned int mpid;

	assert(core_pos < PLATFORM_CORE_COUNT);

	mpid = make_mpid(
			sgi575_cores[core_pos].cluster_id,
			sgi575_cores[core_pos].cpu_id);

	return mpid;
}
