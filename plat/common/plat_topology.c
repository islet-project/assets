/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <assert.h>
#include <debug.h>
#include <plat_topology.h>
#include <platform.h>
#include <stdlib.h>

#define CPU_INDEX_IS_VALID(_cpu_idx)	\
	(((_cpu_idx) - tftf_pwr_domain_start_idx[0]) < PLATFORM_CORE_COUNT)

#define IS_A_CPU_NODE(_cpu_idx)		(tftf_pd_nodes[(_cpu_idx)].level == 0)

#define CPU_NODE_IS_VALID(_cpu_node)	\
	(CPU_INDEX_IS_VALID(_cpu_node) && IS_A_CPU_NODE(_cpu_node))

/*
 * Global variable to check that the platform topology is not queried until it
 * has been setup.
 */
static unsigned int topology_setup_done;

/*
 * Store the start indices of power domains at various levels. This array makes it
 * easier to traverse the topology tree if the power domain level is known.
 */
unsigned int tftf_pwr_domain_start_idx[PLATFORM_MAX_AFFLVL + 1];

/* The grand array to store the platform power domain topology */
tftf_pwr_domain_node_t tftf_pd_nodes[PLATFORM_NUM_AFFS];

#if DEBUG
/*
 * Debug function to display the platform topology.
 * Does not print absent affinity instances.
 */
static void dump_topology(void)
{
	unsigned int cluster_idx, cpu_idx, count;

	NOTICE("Platform topology:\n");

	NOTICE("  %u cluster(s)\n", tftf_get_total_clusters_count());
	NOTICE("  %u CPU(s) (total)\n\n", tftf_get_total_cpus_count());

	for (cluster_idx = PWR_DOMAIN_INIT;
	     cluster_idx = tftf_get_next_peer_domain(cluster_idx, 1),
	     cluster_idx != PWR_DOMAIN_INIT;) {
		count = 0;
		for (cpu_idx = tftf_pd_nodes[cluster_idx].cpu_start_node;
		     cpu_idx < (tftf_pd_nodes[cluster_idx].cpu_start_node +
				     tftf_pd_nodes[cluster_idx].ncpus);
		     cpu_idx++) {
			if (tftf_pd_nodes[cpu_idx].is_present)
				count++;
		}
		NOTICE("  Cluster #%u   [%u CPUs]\n",
				cluster_idx - tftf_pwr_domain_start_idx[1],
				count);
		for (cpu_idx = PWR_DOMAIN_INIT;
		     cpu_idx = tftf_get_next_cpu_in_pwr_domain(cluster_idx, cpu_idx),
		     cpu_idx != PWR_DOMAIN_INIT;) {
			NOTICE("    CPU #%u   [MPID: 0x%x]\n",
					cpu_idx - tftf_pwr_domain_start_idx[0],
					tftf_get_mpidr_from_node(cpu_idx));
		}
	}
	NOTICE("\n");
}
#endif

unsigned int tftf_get_total_aff_count(unsigned int aff_lvl)
{
	unsigned int count = 0;
	unsigned int node_idx;

	assert(topology_setup_done == 1);

	if (aff_lvl > PLATFORM_MAX_AFFLVL)
		return count;

	node_idx = tftf_pwr_domain_start_idx[aff_lvl];

	while (tftf_pd_nodes[node_idx].level == aff_lvl) {
		if (tftf_pd_nodes[node_idx].is_present)
			count++;
		node_idx++;
	}

	return count;
}

unsigned int tftf_get_next_peer_domain(unsigned int pwr_domain_idx,
				      unsigned int pwr_lvl)
{
	assert(topology_setup_done == 1);

	assert(pwr_lvl <= PLATFORM_MAX_AFFLVL);

	if (pwr_domain_idx == PWR_DOMAIN_INIT) {
		pwr_domain_idx = tftf_pwr_domain_start_idx[pwr_lvl];
		if (tftf_pd_nodes[pwr_domain_idx].is_present)
			return pwr_domain_idx;
	}

	assert(pwr_domain_idx < PLATFORM_NUM_AFFS &&
			tftf_pd_nodes[pwr_domain_idx].level == pwr_lvl);

	for (++pwr_domain_idx; (pwr_domain_idx < PLATFORM_NUM_AFFS)
				&& (tftf_pd_nodes[pwr_domain_idx].level == pwr_lvl);
				pwr_domain_idx++) {
		if (tftf_pd_nodes[pwr_domain_idx].is_present)
			return pwr_domain_idx;
	}

	return PWR_DOMAIN_INIT;
}

unsigned int tftf_get_next_cpu_in_pwr_domain(unsigned int pwr_domain_idx,
				      unsigned int cpu_node)
{
	unsigned int cpu_end_node;

	assert(topology_setup_done == 1);
	assert(pwr_domain_idx != PWR_DOMAIN_INIT
			&& pwr_domain_idx < PLATFORM_NUM_AFFS);

	if (cpu_node == PWR_DOMAIN_INIT) {
		cpu_node = tftf_pd_nodes[pwr_domain_idx].cpu_start_node;
		if (tftf_pd_nodes[cpu_node].is_present)
			return cpu_node;
	}

	assert(CPU_NODE_IS_VALID(cpu_node));

	cpu_end_node = tftf_pd_nodes[pwr_domain_idx].cpu_start_node
			+ tftf_pd_nodes[pwr_domain_idx].ncpus - 1;

	assert(cpu_end_node < PLATFORM_NUM_AFFS);

	for (++cpu_node; cpu_node <= cpu_end_node; cpu_node++) {
		if (tftf_pd_nodes[cpu_node].is_present)
			return cpu_node;
	}

	return PWR_DOMAIN_INIT;
}

/*
 * Helper function to get the parent nodes of a particular CPU power
 * domain.
 */
static void get_parent_pwr_domain_nodes(unsigned int cpu_node,
				      unsigned int end_lvl,
				      unsigned int node_index[])
{
	unsigned int parent_node = tftf_pd_nodes[cpu_node].parent_node;
	unsigned int i;

	for (i = 1; i <= end_lvl; i++) {
		node_index[i - 1] = parent_node;
		parent_node = tftf_pd_nodes[parent_node].parent_node;
	}
}

/*******************************************************************************
 * This function updates cpu_start_node and ncpus field for each of the nodes
 * in tftf_pd_nodes[]. It does so by comparing the parent nodes of each of
 * the CPUs and check whether they match with the parent of the previous
 * CPU. The basic assumption for this work is that children of the same parent
 * are allocated adjacent indices. The platform should ensure this through
 * proper mapping of the CPUs to indices via platform_get_core_pos() API.
 *
 * It also updates the 'is_present' field for non-cpu power domains. It does
 * this by checking the 'is_present' field of the child cpu nodes and updates
 * it if any of the child cpu nodes are present.
 *******************************************************************************/
static void update_pwrlvl_limits(void)
{
	int cpu_id, j, is_present;
	unsigned int nodes_idx[PLATFORM_MAX_AFFLVL] = {-1};
	unsigned int temp_index[PLATFORM_MAX_AFFLVL];

	unsigned int cpu_node_offset = tftf_pwr_domain_start_idx[0];

	for (cpu_id = 0; cpu_id < PLATFORM_CORE_COUNT; cpu_id++) {
		get_parent_pwr_domain_nodes(cpu_id + cpu_node_offset,
						PLATFORM_MAX_AFFLVL,
						temp_index);
		is_present = tftf_pd_nodes[cpu_id + cpu_node_offset].is_present;

		for (j = PLATFORM_MAX_AFFLVL - 1; j >= 0; j--) {
			if (temp_index[j] != nodes_idx[j]) {
				nodes_idx[j] = temp_index[j];
				tftf_pd_nodes[nodes_idx[j]].cpu_start_node
							= cpu_id + cpu_node_offset;
				if (!tftf_pd_nodes[nodes_idx[j]].is_present)
					tftf_pd_nodes[nodes_idx[j]].is_present = is_present;
			}
			tftf_pd_nodes[nodes_idx[j]].ncpus++;
		}
	}
}

/******************************************************************************
 * This function populates the power domain topology array 'tftf_pd_nodes[]'
 * based on the power domain description retrieved from the platform layer.
 * It also updates the start index of each power domain level in
 * tftf_pwr_domain_start_idx[]. The uninitialized fields of 'tftf_pd_nodes[]'
 * for the non CPU power domain will be initialized in update_pwrlvl_limits().
 *****************************************************************************/
static void populate_power_domain_tree(void)
{
	unsigned int i, j = 0, num_nodes_at_lvl = 1, num_nodes_at_next_lvl,
			node_index = 0, parent_idx = 0, num_children;
	int num_level = PLATFORM_MAX_AFFLVL;
	const unsigned char *plat_array;

	plat_array = tftf_plat_get_pwr_domain_tree_desc();

	/*
	 * For each level the inputs are:
	 * - number of nodes at this level in plat_array i.e. num_nodes_at_lvl
	 *   This is the sum of values of nodes at the parent level.
	 * - Index of first entry at this level in the plat_array i.e.
	 *   parent_idx.
	 * - Index of first free entry in tftf_pd_nodes[].
	 */
	while (num_level >= 0) {
		num_nodes_at_next_lvl = 0;

		/* Store the start index for every level */
		tftf_pwr_domain_start_idx[num_level] = node_index;

		/*
		 * For each entry (parent node) at this level in the plat_array:
		 * - Find the number of children
		 * - Allocate a node in a power domain array for each child
		 * - Set the parent of the child to the parent_node_index - 1
		 * - Increment parent_node_index to point to the next parent
		 * - Accumulate the number of children at next level.
		 */
		for (i = 0; i < num_nodes_at_lvl; i++) {
			assert(parent_idx <=
				PLATFORM_NUM_AFFS - PLATFORM_CORE_COUNT);
			num_children = plat_array[parent_idx];

			for (j = node_index;
				j < node_index + num_children; j++) {
				/* Initialize the power domain node */
				tftf_pd_nodes[j].parent_node = parent_idx - 1;
				tftf_pd_nodes[j].level = num_level;

				/* Additional initializations for CPU power domains */
				if (num_level == 0) {
					/* Calculate the cpu id from node index */
					int cpu_id =  j - tftf_pwr_domain_start_idx[0];

					assert(cpu_id < PLATFORM_CORE_COUNT);

					/* Set the mpidr of cpu node */
					tftf_pd_nodes[j].mpidr =
						tftf_plat_get_mpidr(cpu_id);
					if (tftf_pd_nodes[j].mpidr != INVALID_MPID)
						tftf_pd_nodes[j].is_present = 1;

					tftf_pd_nodes[j].cpu_start_node = j;
					tftf_pd_nodes[j].ncpus = 1;
				}
			}
			node_index = j;
			num_nodes_at_next_lvl += num_children;
			parent_idx++;
		}

		num_nodes_at_lvl = num_nodes_at_next_lvl;
		num_level--;
	}

	/* Validate the sanity of array exported by the platform */
	assert(j == PLATFORM_NUM_AFFS);
}


void tftf_init_topology(void)
{
	populate_power_domain_tree();
	update_pwrlvl_limits();
	topology_setup_done = 1;
#if DEBUG
	dump_topology();
#endif
}

unsigned int tftf_topology_next_cpu(unsigned int cpu_node)
{
	assert(topology_setup_done == 1);

	if (cpu_node == PWR_DOMAIN_INIT) {
		cpu_node = tftf_pwr_domain_start_idx[0];
		if (tftf_pd_nodes[cpu_node].is_present)
			return cpu_node;
	}

	assert(CPU_NODE_IS_VALID(cpu_node));

	for (++cpu_node; cpu_node < PLATFORM_NUM_AFFS; cpu_node++) {
		if (tftf_pd_nodes[cpu_node].is_present)
			return cpu_node;
	}

	return PWR_DOMAIN_INIT;
}


unsigned int tftf_get_mpidr_from_node(unsigned int cpu_node)
{
	assert(topology_setup_done == 1);

	assert(CPU_NODE_IS_VALID(cpu_node));

	if (tftf_pd_nodes[cpu_node].is_present)
		return tftf_pd_nodes[cpu_node].mpidr;

	return INVALID_MPID;
}

unsigned int tftf_find_any_cpu_other_than(unsigned exclude_mpid)
{
	unsigned int cpu_node, mpidr;

	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr != exclude_mpid)
			return mpidr;
	}

	return INVALID_MPID;
}

unsigned int tftf_find_random_cpu_other_than(unsigned int exclude_mpid)
{
	unsigned int cpu_node, mpidr;
	unsigned int possible_cpus_cnt = 0;
	unsigned int possible_cpus[PLATFORM_CORE_COUNT];

	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr != exclude_mpid)
			possible_cpus[possible_cpus_cnt++] = mpidr;
	}

	if (possible_cpus_cnt == 0)
		return INVALID_MPID;

	return possible_cpus[rand() % possible_cpus_cnt];
}
