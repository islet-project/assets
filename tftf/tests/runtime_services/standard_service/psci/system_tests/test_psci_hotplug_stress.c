/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <debug.h>
#include <events.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <test_helpers.h>
#include <tftf.h>
#include <tftf_lib.h>

#define STRESS_TEST_COUNT 1000

/* TODO: Remove assumption that affinity levels always map to power levels. */
#define MPIDR_CLUSTER_ID(mpid)	MPIDR_AFF_ID(mpid, 1)
#define MPIDR_CPU_ID(mpid)	MPIDR_AFF_ID(mpid, 0)

static event_t cpu_booted[PLATFORM_CORE_COUNT];
static event_t cluster_booted;

/* Return success depicting CPU booted successfully */
static test_result_t test_cpu_booted(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	return TEST_RESULT_SUCCESS;
}

/* Return success depicting all cores in a cluster booted successfully */
static test_result_t test_cluster_booted(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	tftf_wait_for_event(&cluster_booted);

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Random hotplug cores in a large iteration to stress boot path code
 * Test Do :
 * 1) Power up a random core
 * 2) Ensure this core has booted successfully to TFTF
 * 3) Wait for core to be powered off by the framework.
 * 4) Repeat 1-2-3 STRESS_TEST_COUNT times
 * 5) The test is aborted straight away if any failure occurs. In this case,
 *    the test is declared as failed.
 * Note: It will be skipped on single-core platforms.
 */
test_result_t psci_hotplug_single_core_stress_test(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu;
	unsigned int core_pos;
	int psci_ret;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	NOTICE("Power on and off any random core %d times\n",
							STRESS_TEST_COUNT);

	for (unsigned int i = 0; i < STRESS_TEST_COUNT; ++i) {
		/* Reset/Initialise the event variable */
		for (unsigned int j = 0; j < PLATFORM_CORE_COUNT; ++j)
			tftf_init_event(&cpu_booted[j]);

		/*
		 * Find a random CPU to power up and power down
		 */
		cpu = tftf_find_random_cpu_other_than(lead_mpid);
		assert(cpu != lead_mpid);

		psci_ret = tftf_cpu_on(cpu,
					(uintptr_t) test_cpu_booted,
					0);
		if (psci_ret != PSCI_E_SUCCESS)
			return TEST_RESULT_FAIL;

		core_pos = platform_get_core_pos(cpu);
		tftf_wait_for_event(&cpu_booted[core_pos]);

		/*
		 * Wait for the CPU to be powered off by framework before issuing a
		 * CPU_ON to it
		 */
		while (tftf_is_cpu_online(cpu))
			;
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Repeated cores hotplug as stress test
 * Test Do :
 * 1) Power up all the cores
 * 2) Ensure all the cores have booted successfully to TFTF
 * 3) Wait for all the cores to be powered off by the framework.
 * 4) Repeat 1-2-3 STRESS_TEST_COUNT times
 * 5) The test is aborted straight away if any failure occurs. In this case,
 *    the test is declared as failed.
 * Note: It will be skipped on single-core platforms.
 */
test_result_t psci_hotplug_stress_test(void)
{
	unsigned int lead_cpu = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_mpid, cpu_node;
	unsigned int core_pos;
	int psci_ret;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	NOTICE("This multi-core test will repeat %d times\n",
							STRESS_TEST_COUNT);

	for (unsigned int i = 0; i < STRESS_TEST_COUNT; i++) {
		/* Reinitialize the event variable */
		for (unsigned int j = 0; j < PLATFORM_CORE_COUNT; ++j)
			tftf_init_event(&cpu_booted[j]);

		for_each_cpu(cpu_node) {
			cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
			/* Skip lead CPU, it is already powered on */
			if (cpu_mpid == lead_cpu)
				continue;

			psci_ret = tftf_cpu_on(cpu_mpid,
					(uintptr_t) test_cpu_booted,
					0);
			if (psci_ret != PSCI_E_SUCCESS)
				return TEST_RESULT_FAIL;
		}

		/*
		 * Confirm the non-lead cpus booted and participated in the test
		 */
		for_each_cpu(cpu_node) {
			cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
			/* Skip lead CPU */
			if (cpu_mpid == lead_cpu)
				continue;

			core_pos = platform_get_core_pos(cpu_mpid);
			tftf_wait_for_event(&cpu_booted[core_pos]);
		}

		for_each_cpu(cpu_node) {
			cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
			/*
			 * Except lead CPU, Wait for all cores to be powered off
			 * by framework
			 */
			if (cpu_mpid == lead_cpu)
				continue;

			while (tftf_is_cpu_online(cpu_mpid))
				;
		}
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Stress test cluster hotplug
 * Test Do :
 * 1) Power up all the cores of non-lead cluster
 * 2) Ensure all the cores have booted successfully to TFTF
 * 3) Wait for all the cores to be powered off by the framework.
 * 4) Repeat 1-2-3 STRESS_TEST_COUNT times
 * 5) The test is aborted straight away if any failure occurs. In this case,
 *    the test is declared as failed.
 * Note: It will be skipped on single-cluster platforms.
 */
test_result_t psci_cluster_hotplug_stress_test(void)
{
	unsigned int lead_cluster = MPIDR_CLUSTER_ID(read_mpidr_el1());
	unsigned int cpu_mpid, cpu_node;
	unsigned int core_pos;
	int psci_ret;

	SKIP_TEST_IF_LESS_THAN_N_CLUSTERS(2);

	NOTICE("This Cluster hotplug test will repeat %d times\n",
							STRESS_TEST_COUNT);

	for (unsigned int i = 0; i < STRESS_TEST_COUNT; i++) {
		/* Reset/Initialise the event variable */
		tftf_init_event(&cluster_booted);

		/* Reset/Initialise the event variable */
		for (unsigned int j = 0; j < PLATFORM_CORE_COUNT; ++j)
			tftf_init_event(&cpu_booted[j]);

		for_each_cpu(cpu_node) {
			cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
			if (MPIDR_CLUSTER_ID(cpu_mpid) != lead_cluster) {
				psci_ret = tftf_cpu_on(cpu_mpid,
						(uintptr_t) test_cluster_booted,
						       0);
				if (psci_ret != PSCI_E_SUCCESS)
					return TEST_RESULT_FAIL;
			}
		}

		/*
		 * Confirm all the CPU's in non-lead cluster booted
		 * and participated in the test
		 */
		for_each_cpu(cpu_node) {
			cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
			/* Skip lead CPU cluster */
			if (MPIDR_CLUSTER_ID(cpu_mpid) == lead_cluster)
				continue;

			core_pos = platform_get_core_pos(cpu_mpid);
			tftf_wait_for_event(&cpu_booted[core_pos]);
		}

		/*
		 * All cores have booted, Now send the signal to them so that
		 * they enter the framework
		 */
		tftf_send_event_to_all(&cluster_booted);

		for_each_cpu(cpu_node) {
			cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
			if (MPIDR_CLUSTER_ID(cpu_mpid) != lead_cluster)
				/*
				 * Wait for CPU to power off before issuing a
				 * CPU_ON for it
				 */
				while (tftf_is_cpu_online(cpu_mpid))
					;
		}
	}

	return TEST_RESULT_SUCCESS;
}
