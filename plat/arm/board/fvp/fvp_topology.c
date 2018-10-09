/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <assert.h>
#include <mmio.h>
#include <plat_topology.h>
#include <platform_def.h>
#include <stddef.h>
#include <tftf_lib.h>

/* FVP Power controller based defines */
#define PWRC_BASE		0x1c100000
#define PSYSR_OFF		0x10
#define PSYSR_INVALID		0xffffffff

static const struct {
	unsigned int cluster_id;
	unsigned int cpu_id;
} fvp_base_aemv8a_aemv8a_cores[] = {
	/* Cluster 0 */
	{ 0, 0 },
	{ 0, 1 },
	{ 0, 2 },
	{ 0, 3 },
	/* Cluster 1 */
	{ 1, 0 },
	{ 1, 1 },
	{ 1, 2 },
	{ 1, 3 },
	/* Cluster 2 */
	{ 2, 0 },
	{ 2, 1 },
	{ 2, 2 },
	{ 2, 3 },
	/* Cluster 3 */
	{ 3, 0 },
	{ 3, 1 },
	{ 3, 2 },
	{ 3, 3 },
};

/*
 * The FVP power domain tree descriptor. We always construct a topology
 * with the maximum number of cluster nodes possible for FVP. During
 * TFTF initialization, the actual number of nodes present on the model
 * will be queried dynamically using `tftf_plat_get_mpidr()`.
 * The FVP power domain tree does not have a single system level power domain
 * i.e. a single root node. The first entry in the power domain descriptor
 * specifies the number of power domains at the highest power level which
 * is equal to FVP_CLUSTER_COUNT.
 */
static const unsigned char fvp_power_domain_tree_desc[] = {
	/* Number of system nodes */
	1,
	/* Number of cluster nodes */
	FVP_CLUSTER_COUNT,
	/* Number of children for the first node */
	FVP_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the second node */
	FVP_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the third node */
	FVP_MAX_CPUS_PER_CLUSTER,
	/* Number of children for the fourth node */
	FVP_MAX_CPUS_PER_CLUSTER
};

const unsigned char *tftf_plat_get_pwr_domain_tree_desc(void)
{
	return fvp_power_domain_tree_desc;
}

static unsigned int fvp_pwrc_read_psysr(unsigned long mpidr)
{
	unsigned int rc;
	mmio_write_32(PWRC_BASE + PSYSR_OFF, (unsigned int) mpidr);
	rc = mmio_read_32(PWRC_BASE + PSYSR_OFF);
	return rc;
}

uint64_t tftf_plat_get_mpidr(unsigned int core_pos)
{
	unsigned int mpid;

	assert(core_pos < PLATFORM_CORE_COUNT);

	mpid = make_mpid(
			fvp_base_aemv8a_aemv8a_cores[core_pos].cluster_id,
			fvp_base_aemv8a_aemv8a_cores[core_pos].cpu_id);

	if (fvp_pwrc_read_psysr(mpid) != PSYSR_INVALID)
		return mpid;

	return INVALID_MPID;
}
