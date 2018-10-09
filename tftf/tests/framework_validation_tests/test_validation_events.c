/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <events.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <psci.h>
#include <tftf_lib.h>

/* Events structures used by this test case */
static event_t lead_cpu_event;
static event_t cpu_has_entered_test[PLATFORM_CORE_COUNT];
static event_t test_is_finished;

static test_result_t non_lead_cpu_fn(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Signal to the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_has_entered_test[core_pos]);

	tftf_wait_for_event(&lead_cpu_event);

	/*
	 * Wait for lead CPU's signal before exiting the test.
	 * Introduce a delay so that the lead CPU will send the event before the
	 * non-lead CPUs wait for it.
	 */
	waitms(500);
	tftf_wait_for_event(&test_is_finished);

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Validate the events API
 *
 * This test exercises the events API.
 * - It creates a sequence of events sending and receiving. The order of
 *   operations is ensured by inserting delays at strategic points.
 * - It tests the communication in both directions (i.e. from CPUx to CPUy and
 *   vice versa).
 * - It tests that it doesn't matter whether CPUx waits for the event first
 *   then CPUy sends the event, or that things happen in the other order.
 * - It tests the API on a single CPU.
 *
 * This test is skipped if an error occurs during the bring-up of non-lead CPUs.
 * Otherwise, this test always returns success. If something goes wrong, the
 * test will most probably hang because the system will go into a WFE/SEV dead
 * lock.
 */
test_result_t test_validation_events(void)
{
	unsigned int lead_cpu;
	unsigned int cpu_mpid;
	unsigned int cpu_node;
	unsigned int core_pos;
	unsigned int cpus_count;
	int psci_ret;

	lead_cpu = read_mpidr_el1() & MPID_MASK;

	/*
	 * The events API should work on a single CPU, provided that the event
	 * is sent before we wait for it. If we do things the other way around,
	 * the CPU will end up stuck in WFE state.
	 */
	tftf_send_event(&lead_cpu_event);
	tftf_wait_for_event(&lead_cpu_event);

	/* Re-init lead_cpu_event to be able to reuse it */
	tftf_init_event(&lead_cpu_event);

	/* Power on all CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already powered on */
		if (cpu_mpid == lead_cpu)
			continue;

		psci_ret = tftf_cpu_on(cpu_mpid, (uintptr_t) non_lead_cpu_fn, 0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU 0x%x (%d)\n",
				cpu_mpid, psci_ret);
			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for all CPUs to have entered the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (cpu_mpid == lead_cpu)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_has_entered_test[core_pos]);
	}

	/*
	 * Introduce a delay so that the non-lead CPUs will wait for this event
	 * before the lead CPU sends it.
	 */
	waitms(500);
	/* Send the event to half of the CPUs */
	cpus_count = PLATFORM_CORE_COUNT / 2;
	tftf_send_event_to(&lead_cpu_event, cpus_count);
	waitms(500);
	/* Send the event to the other half of the CPUs */
	tftf_send_event_to(&lead_cpu_event, PLATFORM_CORE_COUNT - cpus_count);

	/* Signal termination of the test to all CPUs */
	tftf_send_event_to_all(&test_is_finished);

	return TEST_RESULT_SUCCESS;
}
