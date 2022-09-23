/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <plat_topology.h>
#include <tftf_lib.h>

static const struct {
	unsigned int cluster_id;
	unsigned int cpu_id;
} plat_cores[] = {
	/* Cluster0: 1 core */
	{ 0, 0 },
	/* Cluster1: 1 core */
	{ 1, 0 },
	/* Cluster2: 1 core */
	{ 2, 0 },
	/* Cluster3: 1 core */
	{ 3, 0 },
	/* Cluster4: 1 core */
	{ 4, 0 },
	/* Cluster5: 1 core */
	{ 5, 0 },
	/* Cluster6: 1 core */
	{ 6, 0 },
	/* Cluster7: 1 core */
	{ 7, 0 },
#if (CSS_SGI_PLATFORM_VARIANT == 0)
	/* Cluster8: 1 core */
	{ 8, 0 },
	/* Cluster9: 1 core */
	{ 9, 0 },
	/* Cluster10: 1 core */
	{ 10, 0 },
	/* Cluster11: 1 core */
	{ 11, 0 },
	/* Cluster12: 1 core */
	{ 12, 0 },
	/* Cluster13: 1 core */
	{ 13, 0 },
	/* Cluster14: 1 core */
	{ 14, 0 },
	/* Cluster15: 1 core */
	{ 15, 0 },
#endif
};

/*
 * The power domain tree descriptor. The cluster power domains are
 * arranged so that when the PSCI generic code creates the power domain tree,
 * the indices of the CPU power domain nodes it allocates match the linear
 * indices returned by plat_core_pos_by_mpidr().
 */
const unsigned char plat_pd_tree_desc[] = {
	/* Number of root nodes */
	PLAT_ARM_CLUSTER_COUNT,
	/* Number of children for the 1st node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 2nd node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 3rd node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 4th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 5th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 6th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 7th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 8th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
#if (CSS_SGI_PLATFORM_VARIANT == 0)
	/* Number of children for the 9th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 10th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 11th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 12th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 13th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 14th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 15th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the 16th node */
	CSS_SGI_MAX_CPUS_PER_CLUSTER
#endif
};

const unsigned char *tftf_plat_get_pwr_domain_tree_desc(void)
{
	return plat_pd_tree_desc;
}

uint64_t tftf_plat_get_mpidr(unsigned int core_pos)
{
	unsigned int mpid;

	assert(core_pos < PLATFORM_CORE_COUNT);

	mpid = make_mpid(plat_cores[core_pos].cluster_id,
				plat_cores[core_pos].cpu_id);

	return (uint64_t)mpid;
}
