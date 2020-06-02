/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
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

#if FVP_MAX_PE_PER_CPU == 2
/* SMT: 2 threads per CPU */
#define	CPU_DEF(cluster, cpu)	\
	{ cluster, cpu, 0 },	\
	{ cluster, cpu, 1 }
#else
/* ST: 1 thread per CPU */
#define	CPU_DEF(cluster, cpu)	\
	{ cluster, cpu }
#endif	/* FVP_MAX_PE_PER_CPU */

/*
 * Max CPUs per cluster:
 * ST:	4
 * SMT:	8
 */
#if (FVP_MAX_CPUS_PER_CLUSTER == 1)
#define	CLUSTER_DEF(cluster)	\
	CPU_DEF(cluster, 0)
#elif (FVP_MAX_CPUS_PER_CLUSTER == 2)
#define	CLUSTER_DEF(cluster)	\
	CPU_DEF(cluster, 0),	\
	CPU_DEF(cluster, 1)
#elif (FVP_MAX_CPUS_PER_CLUSTER == 3)
#define	CLUSTER_DEF(cluster)	\
	CPU_DEF(cluster, 0),	\
	CPU_DEF(cluster, 1),	\
	CPU_DEF(cluster, 2)
#elif (FVP_MAX_CPUS_PER_CLUSTER == 4)
#define	CLUSTER_DEF(cluster)	\
	CPU_DEF(cluster, 0),	\
	CPU_DEF(cluster, 1),	\
	CPU_DEF(cluster, 2),	\
	CPU_DEF(cluster, 3)
#elif (FVP_MAX_CPUS_PER_CLUSTER == 5)
#define	CLUSTER_DEF(cluster)	\
	CPU_DEF(cluster, 0),	\
	CPU_DEF(cluster, 1),	\
	CPU_DEF(cluster, 2),	\
	CPU_DEF(cluster, 3),	\
	CPU_DEF(cluster, 4)
#elif (FVP_MAX_CPUS_PER_CLUSTER == 6)
#define	CLUSTER_DEF(cluster)	\
	CPU_DEF(cluster, 0),	\
	CPU_DEF(cluster, 1),	\
	CPU_DEF(cluster, 2),	\
	CPU_DEF(cluster, 3),	\
	CPU_DEF(cluster, 4),	\
	CPU_DEF(cluster, 5)
#elif (FVP_MAX_CPUS_PER_CLUSTER == 7)
#define	CLUSTER_DEF(cluster)	\
	CPU_DEF(cluster, 0),	\
	CPU_DEF(cluster, 1),	\
	CPU_DEF(cluster, 2),	\
	CPU_DEF(cluster, 3),	\
	CPU_DEF(cluster, 4),	\
	CPU_DEF(cluster, 5),	\
	CPU_DEF(cluster, 6)
#else
#define	CLUSTER_DEF(cluster)	\
	CPU_DEF(cluster, 0),	\
	CPU_DEF(cluster, 1),	\
	CPU_DEF(cluster, 2),	\
	CPU_DEF(cluster, 3),	\
	CPU_DEF(cluster, 4),	\
	CPU_DEF(cluster, 5),	\
	CPU_DEF(cluster, 6),	\
	CPU_DEF(cluster, 7)
#endif	/* FVP_MAX_CPUS_PER_CLUSTER */

static const struct {
	unsigned int cluster_id;
	unsigned int cpu_id;
#if FVP_MAX_PE_PER_CPU > 1
	unsigned int thread_id;
#endif
} fvp_base_aemv8a_aemv8a_cores[] = {
	/* Clusters 0...3 */
	CLUSTER_DEF(0),
#if (FVP_CLUSTER_COUNT > 1)
	CLUSTER_DEF(1),
#if (FVP_CLUSTER_COUNT > 2)
	CLUSTER_DEF(2),
#if (FVP_CLUSTER_COUNT > 3)
	CLUSTER_DEF(3)
#endif
#endif
#endif
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
	FVP_MAX_CPUS_PER_CLUSTER * FVP_MAX_PE_PER_CPU,
#if (FVP_CLUSTER_COUNT > 1)
	/* Number of children for the second node */
	FVP_MAX_CPUS_PER_CLUSTER * FVP_MAX_PE_PER_CPU,
#if (FVP_CLUSTER_COUNT > 2)
	/* Number of children for the third node */
	FVP_MAX_CPUS_PER_CLUSTER * FVP_MAX_PE_PER_CPU,
#if (FVP_CLUSTER_COUNT > 3)
	/* Number of children for the fourth node */
	FVP_MAX_CPUS_PER_CLUSTER * FVP_MAX_PE_PER_CPU
#endif
#endif
#endif
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
#if FVP_MAX_PE_PER_CPU > 1
			fvp_base_aemv8a_aemv8a_cores[core_pos].cpu_id,
			fvp_base_aemv8a_aemv8a_cores[core_pos].thread_id);
#else
			fvp_base_aemv8a_aemv8a_cores[core_pos].cpu_id);
#endif

	if (fvp_pwrc_read_psysr(mpid) != PSYSR_INVALID)
		return mpid;

	return INVALID_MPID;
}
