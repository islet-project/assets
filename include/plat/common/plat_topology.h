/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLAT_TOPOLOGY_H__
#define __PLAT_TOPOLOGY_H__

#include <arch.h>
#include <platform_def.h>
#include <stdint.h>

/*
 * This is the initial value of the power domain index when used
 * as argument to the tftf topology helpers. They are also used
 * to indicate the end of iterative topology navigation when returned
 * by the tftf topology helpers.
 */
#define PWR_DOMAIN_INIT		((unsigned int) -1)

/*
 * Return the total number of clusters in the system.
 * Currently there is correspondence between power level and affinity
 * level and hence cluster power level corresponds to affinity level 1.
 */
#define tftf_get_total_clusters_count()		tftf_get_total_aff_count(1)

/*
 * Return the total number of CPUs in the system (across all clusters).
 * Currently there is correspondence between power level and affinity
 * level and hence CPU power level correspond to affinity level 0.
 */
#define tftf_get_total_cpus_count()		tftf_get_total_aff_count(0)

/*
 * Converts a 'core_pos' into an MPIDR. The 'core_pos' is a unique number
 * corresponding to a CPU as returned by platform_get_core_pos() API
 */
#define tftf_core_pos_to_mpidr(core_pos)	\
			tftf_get_mpidr_from_node(core_pos + tftf_pwr_domain_start_idx[0])

/*
 * The following array stores the start index of each level in the power
 * domain topology tree.
 */
extern unsigned int tftf_pwr_domain_start_idx[PLATFORM_MAX_AFFLVL + 1];

/*
 * The following data structure represents a TFTF power domain node.
 */
typedef struct tftf_pwr_domain_node {
	/*
	 * Array index of the first CPU in the topology array for which this
	 * power domain is the parent. If this power domain is a CPU, then
	 * `cpu_start_node` will be its own index in the power domain
	 * topology array.
	 */
	unsigned int cpu_start_node;

	/*
	 * Number of CPU power domains which belong to this power domain.
	 * i.e. all the CPUs in the range 'cpu_start_node
	 * -> cpu_start_node + ncpus - 1 ' will belong to this power domain.
	 * If this power domain is a CPU then 'ncpus' will be 1.
	 */
	unsigned int ncpus;

	/* Valid only for CPU power domains */
	unsigned int mpidr;

	/* Index of the parent power domain node */
	unsigned int parent_node;

	/*
	 * The level of this power domain node in the power domain topology
	 * tree. It could correspond to the affinity level but the platform
	 * could have power levels which do not match affinity levels.
	 */
	unsigned int level;

	/*
	 * The 'is_present' field is used to cater for power domains
	 * which are absent.
	 */
	unsigned char is_present;
} tftf_pwr_domain_node_t;

/*
 * Detect and store the platform topology so that test cases can query it later.
 */
void tftf_init_topology(void);

/*
 * Return the number of affinity instances implemented at the affinity level
 * passed as an argument. This function returns 0 for any other affinity
 * level higher than that supported by the platform.
 */
unsigned int tftf_get_total_aff_count(unsigned int aff_lvl);

/*
 * Returns the index of the next power domain after `pwr_domain_idx`
 * in the topology tree at the same `aff_lvl`. The `pwr_domain_idx`
 * has to be initialized to PWR_DOMAIN_INIT to get the first entry.
 * It returns PWR_DOMAIN_INIT if none is found.
 */
unsigned int tftf_get_next_peer_domain(unsigned int pwr_domain_idx,
				      unsigned int pwr_lvl);

/*
 * Returns the index of the next CPU after the current CPU `cpu_node`
 * which belongs to the power domain `pwr_domain_idx`. The `cpu_node`
 * has to be initialized to PWR_DOMAIN_INIT to get the first entry.
 * It returns PWR_DOMAIN_INIT if none is found.
 */
unsigned int tftf_get_next_cpu_in_pwr_domain(unsigned int pwr_domain_idx,
				      unsigned int cpu_node);

/*
 * Return the node index of the next CPU after 'cpu_node' in the topology tree.
 * Skip absent CPUs.
 * cpu_node: Node index of the current CPU.
 */
unsigned int tftf_topology_next_cpu(unsigned int cpu_node);

/*
 * Iterate over every CPU. Skip absent CPUs.
 * cpu: unsigned integer corresponding to the index of the cpu in
 * the topology array.
 */
#define for_each_cpu(cpu)					\
	for (cpu = tftf_topology_next_cpu(PWR_DOMAIN_INIT);	\
		cpu != PWR_DOMAIN_INIT;				\
		cpu = tftf_topology_next_cpu(cpu))

/*
 * Iterate over every power domain idx for a given level.
 * - idx: unsigned integer corresponding to the power domain index.
 * - lvl: level
 */
#define for_each_power_domain_idx(idx, lvl)				\
	for (idx = tftf_get_next_peer_domain(PWR_DOMAIN_INIT, (lvl));	\
		idx != PWR_DOMAIN_INIT;					\
		idx = tftf_get_next_peer_domain(idx, (lvl)))

/*
 * Iterate over every CPU in a power domain idx.
 * - cpu_idx: CPU index.
 * - pwr_domain_idx: unsigned integer corresponding to the power domain index.
 */
#define for_each_cpu_in_power_domain(cpu_idx, pwr_domain_idx)		\
	for (cpu_idx = tftf_get_next_cpu_in_pwr_domain(			\
			(pwr_domain_idx), PWR_DOMAIN_INIT);		\
		cpu_idx != PWR_DOMAIN_INIT;				\
		cpu_idx = tftf_get_next_cpu_in_pwr_domain(		\
			(pwr_domain_idx), cpu_idx))

/*
 * Returns the MPIDR of the CPU power domain node indexed by `cpu_node`
 * or INVALID_MPID if it is absent.
 */
unsigned int tftf_get_mpidr_from_node(unsigned int cpu_node);

/*
 * Query the platform topology to find another CPU than the one specified
 * as an argument.
 * Return the MPID of this other CPU, or INVALID_MPID if none could be found.
 */
unsigned int tftf_find_any_cpu_other_than(unsigned int exclude_mpid);

/*
 * Query the platform topology to find a random CPU other than the one specified
 * as an argument.
 * The difference between this function and tftf_find_any_cpu_other_than is
 * the randomness in selecting a CPU.
 * Return the MPID of this other CPU, or INVALID_MPID if none could be found.
 */
unsigned int tftf_find_random_cpu_other_than(unsigned int exclude_mpid);

#endif /* __PLAT_TOPOLOGY_H__ */
