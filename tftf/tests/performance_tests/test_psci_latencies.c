/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains tests that measure the latencies for PSCI power
 * down sequences.
 */

#include <arch.h>
#include <arch_helpers.h>
#include <debug.h>
#include <events.h>
#include <irq.h>
#include <mmio.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <stdlib.h>
#include <test_helpers.h>
#include <tftf.h>
#include <tftf_lib.h>

static event_t target_booted, target_keep_on_booted, target_keep_on;

/*
 * Define the variance allowed from baseline number. This is the divisor
 * to be used. For example, for 10% define this macro to 10 and for
 * 20% define to 5.
 */
#define BASELINE_VARIANCE	10

/*
 * Utility function to wait for all CPUs other than the caller to be
 * OFF.
 */
static void wait_for_non_lead_cpus(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int target_mpid, target_node;

	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU, as it is powered on */
		if (target_mpid == lead_mpid)
			continue;

		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0)
				!= PSCI_STATE_OFF)
			;
	}
}

static test_result_t test_target_function(void)
{
	tftf_send_event(&target_booted);
	return TEST_RESULT_SUCCESS;
}

static test_result_t test_target_keep_on_function(void)
{
	tftf_send_event(&target_keep_on_booted);
	tftf_wait_for_event(&target_keep_on);
	return TEST_RESULT_SUCCESS;
}

/*
 * The helper routine for psci_trigger_peer_cluster_cache_coh() test. Turn ON and turn OFF
 * the target CPU while flooding it with CPU ON requests.
 */
static test_result_t get_target_cpu_on_stats(unsigned int target_mpid,
		uint64_t *count_diff, unsigned int *cpu_on_hits_on_target)
{
	int ret;
	uint64_t start_time;

	ret = tftf_try_cpu_on(target_mpid, (uintptr_t) test_target_function, 0);
	if (ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to turn ON target CPU %x\n", target_mpid);
		return TEST_RESULT_FAIL;
	}

	tftf_wait_for_event(&target_booted);

	/* The target CPU is now turning OFF */

	start_time = syscounter_read();

	/* Flood the target CPU with CPU ON requests */
	do {
		ret = tftf_try_cpu_on(target_mpid, (uintptr_t) test_target_function, 0);
		if (ret == PSCI_E_ALREADY_ON)
			(*cpu_on_hits_on_target)++;
	} while ((ret == PSCI_E_ALREADY_ON) && (syscounter_read() < (start_time + read_cntfrq_el0())));

	if (ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("The target failed to turn ON within 1000ms\n");
		return TEST_RESULT_FAIL;
	}

	*count_diff = (uint64_t)(syscounter_read() - start_time);

	tftf_wait_for_event(&target_booted);

	return TEST_RESULT_SUCCESS;
}


/*
 * @Test_Aim@ Measure the difference in latencies in waking up a CPU when it is
 * the last one to powerdown in a cluster vs when the cluster is kept `alive`.
 * This test tries to bring up a CPU on a different cluster and then turn it
 * OFF. As it is being turned OFF, flood it with PSCI_CPU_ON requests from lead
 * CPU and see if there is any delay in detecting that the target CPU is OFF.
 *
 * This test also tries to trigger bug fixed by this patch SHA 71341d23 in TF-A
 * repo. The theory is that when the last CPU in the cluster powers down, it
 * plugs itself out of coherent interconnect and the cache maintenance that was
 * performed as part of the target CPU shutdown may not seen by the current
 * CPU cluster. Hence depending on the cached value of the aff_info state
 * of the target CPU may not be correct. Hence the TF-A patch does a cache line
 * flush before reading the aff_info state in the CPU_ON code path. The test
 * tries to detect whether there is a delay in the aff_info state of the target
 * CPU as seen from the current CPU.
 *
 * The test itself did not manage to trigger the issue on ARM internal
 * platforms but only measures the difference in delay when the
 * last CPU in the cluster is powered down versus `a CPU` in the cluster is
 * powered down.
 *
 * There are 2 parts to this test. In the first part, the target CPU is turned
 * ON and the test is conducted as described above but with another CPU
 * in the target cluster kept running (referred to as the keep ON CPU).
 * The baseline numbers are collected in this configuration.
 *
 * For the second part of the test, the sequence is repeated, but without the
 * `keep on` CPU. The test numbers are collected. If there is a variation of
 * more than BASELINE_VARIANCE from the baseline numbers, then a message
 * indicating the same is printed out. This is a bit subjective test and
 * depends on the platform. Hence this test is not recommended to be run on
 * Models.
 */
test_result_t psci_trigger_peer_cluster_cache_coh(void)
{
	unsigned int target_idx, target_keep_on_idx, cluster_1, cluster_2,
			target_mpid, target_keep_on_mpid, hits_baseline = 0,
			hits_test = 0;
	int ret;
	uint64_t diff_baseline = 0, diff_test = 0;

	SKIP_TEST_IF_LESS_THAN_N_CLUSTERS(2);

	tftf_init_event(&target_booted);
	tftf_init_event(&target_keep_on_booted);
	tftf_init_event(&target_keep_on);

	/* Identify the cluster node corresponding to the lead CPU */
	cluster_1 = tftf_get_parent_node_from_mpidr(read_mpidr_el1(),
					PLAT_MAX_PWR_LEVEL - 1);
	assert(cluster_1 != PWR_DOMAIN_INIT);

	/* Identify 2nd cluster node for the test */
	for_each_power_domain_idx(cluster_2, PLAT_MAX_PWR_LEVEL - 1) {
		if (cluster_2 != cluster_1)
			break;
	}

	assert(cluster_2 != PWR_DOMAIN_INIT);

	/* First lets try to get baseline time data */
	/* Identify a target CPU and `keep_on` CPU nodes on cluster_2 */
	target_idx = tftf_get_next_cpu_in_pwr_domain(cluster_2, PWR_DOMAIN_INIT);
	target_keep_on_idx = tftf_get_next_cpu_in_pwr_domain(cluster_2, target_idx);

	assert(target_idx != PWR_DOMAIN_INIT);

	if (target_keep_on_idx == PWR_DOMAIN_INIT) {
		tftf_testcase_printf("Need at least 2 CPUs on target test cluster\n");
		return TEST_RESULT_SKIPPED;
	}

	/* Get the MPIDR for the target and `keep_on` CPUs */
	target_mpid = tftf_get_mpidr_from_node(target_idx);
	target_keep_on_mpid = tftf_get_mpidr_from_node(target_keep_on_idx);
	assert(target_mpid != INVALID_MPID);
	assert(target_keep_on_mpid != INVALID_MPID);

	/* Turn On the `keep_on CPU` and keep it ON for the baseline data */
	ret = tftf_try_cpu_on(target_keep_on_mpid, (uintptr_t) test_target_keep_on_function, 0);
	if (ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to turn ON target CPU %x\n", target_mpid);
		return TEST_RESULT_FAIL;
	}

	tftf_wait_for_event(&target_keep_on_booted);

	ret = get_target_cpu_on_stats(target_mpid, &diff_baseline, &hits_baseline);

	/* Allow `Keep-on` CPU to power OFF */
	tftf_send_event(&target_keep_on);

	if (ret != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	tftf_testcase_printf("\t\tFinished in ticks \tCPU_ON requests prior to success\n");
	tftf_testcase_printf("Baseline data: \t%lld \t\t\t%d\n", diff_baseline,
							hits_baseline);

	wait_for_non_lead_cpus();

	/*
	 * Now we have baseline data. Try to test the same case but without a
	 * `keep on` CPU.
	 */
	ret = get_target_cpu_on_stats(target_mpid, &diff_test, &hits_test);
	if (ret != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	tftf_testcase_printf("Test data: \t%lld \t\t\t%d\n", diff_test,
							hits_test);

	int variance = ((diff_test - diff_baseline) * 100) / (diff_baseline);
	tftf_testcase_printf("Variance of %d per-cent from baseline detected\n",
			variance);

	wait_for_non_lead_cpus();

	return TEST_RESULT_SUCCESS;
}
