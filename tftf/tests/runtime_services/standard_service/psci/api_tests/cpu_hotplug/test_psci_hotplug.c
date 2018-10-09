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
#include <power_management.h>
#include <psci.h>
#include <test_helpers.h>
#include <tftf_lib.h>

static event_t cpu_booted[PLATFORM_CORE_COUNT];

static test_result_t test_cpu_booted(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	VERBOSE("Hello from core 0x%x\n", mpid);

	return TEST_RESULT_SUCCESS;
}


/*
 * @Test_Aim@ Test CPU hotplug support
 *
 * This test powers on all CPUs using the PSCI CPU_ON API and checks whether the
 * operation succeeded.
 */
test_result_t test_psci_cpu_hotplug(void)
{
	test_result_t ret = TEST_RESULT_SUCCESS;
	unsigned int cpu_node, cpu_mpid;
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos;
	int psci_ret;

	/* Power on all CPUs */
	for_each_cpu(cpu_node) {

		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU, it is already powered on */
		if (cpu_mpid == lead_mpid)
			continue;

		psci_ret = tftf_cpu_on(cpu_mpid,
				(uintptr_t) test_cpu_booted,
				0);
		if (psci_ret != PSCI_E_SUCCESS)
			ret = TEST_RESULT_FAIL;
	}

	/*
	 * The lead CPU needs to wait for all other CPUs to enter the test.
	 * This is because the test framework declares the end of a test when no
	 * CPU is in the test. Therefore, if the lead CPU goes ahead and exits
	 * the test then potentially there could be no CPU executing the test at
	 * this time because none of them have entered the test yet, hence the
	 * framework will be misled in thinking the test is finished.
	 */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_booted[core_pos]);
	}

	return ret;
}

/* Verify context ID passed by lead CPU */
static test_result_t test_context_ids_non_lead_cpu(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Signal to the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	u_register_t ctx_id = tftf_get_cpu_on_ctx_id(core_pos);

	if (ctx_id != (u_register_t)(mpid + core_pos)) {
		tftf_testcase_printf("Failed to get context ID in CPU %d\n",
				     core_pos);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Verify the value of context ID from tftf_cpu_on().
 *
 * This test powers on all the secondary CPUs and sends different context IDs
 * when doing so. All CPUs must receive the correct value without it having
 * been overwritten during the boot process.
 */
test_result_t test_context_ids(void)
{
	int i;
	unsigned int lead_mpid;
	unsigned int cpu_mpid, cpu_node;
	unsigned int core_pos;
	int psci_ret;

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	for (i = 0; i < PLATFORM_CORE_COUNT; i++)
		tftf_init_event(&cpu_booted[i]);

	/* Power on all CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already powered on */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);

		/* Pass as context ID something that the CPU can verify */
		psci_ret = tftf_cpu_on(cpu_mpid,
				       (uintptr_t) test_context_ids_non_lead_cpu,
				       (u_register_t)(cpu_mpid + core_pos));

		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU 0x%x (%d)\n",
				cpu_mpid, psci_ret);
			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for non-lead CPUs to enter the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_booted[core_pos]);
	}

	return TEST_RESULT_SUCCESS;
}
