/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file implements test cases exercising invalid scenarios of the CPU
 * hotplug API. It checks that the PSCI implementation responds as per the
 * PSCI specification.
 */

#include <arch_helpers.h>
#include <events.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <tftf_lib.h>

/*
 * Event data structures used by non-lead CPUs to tell the lead CPU they entered
 * the test.
 */
static event_t entered_test[PLATFORM_CORE_COUNT];

/*
 * If 'real_value' == 'expected_value' then return a test success.
 * Otherwise, print an error message in the test report and report a test
 * failure.
 */
static test_result_t report_result(int expected_value, int real_value)
{
	if (real_value != expected_value) {
		tftf_testcase_printf(
			"Wrong return value, expected %i, got %i\n",
			expected_value, real_value);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

static test_result_t reissue_cpu_hotplug(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);
	int psci_ret;

	tftf_send_event(&entered_test[core_pos]);

	/*
	 * This time, we can't use tftf_cpu_on() to issue the power on request
	 * because this would go through too much test framework logic. E.g. the
	 * framework would figure out that the the CPU is already powered on by
	 * looking at the CPU state information it keeps, hence it would report
	 * an error.
	 *
	 * Here we need to bypass the framework and issue the SMC call directly
	 * from the test case itself. tftf_psci_cpu_on() is a simple wrapper
	 * over the SMC call.
	 *
	 * Entry point address argument can be any valid address.
	 */
	psci_ret = tftf_psci_cpu_on(mpid, (uintptr_t)reissue_cpu_hotplug, 0);

	return report_result(PSCI_E_ALREADY_ON, psci_ret);
}

/*
 * @Test_Aim@ Hotplug request on a CPU which is already powered on
 *
 * 1) Power on all CPUs.
 * 2) Each CPU re-issues the PSCI CPU_ON request on itself. This is expected to
 *    fail and the PSCI implementation is expected to report that CPUs are
 *    already powered on.
 *
 * The test is skipped if an error is encountered during the bring-up of
 * non-lead CPUs.
 */
test_result_t test_psci_cpu_hotplug_plugged(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_mpid, cpu_node;
	int psci_ret;
	unsigned int core_pos;

	/* Power on all CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already powered on */
		if (cpu_mpid == lead_mpid)
			continue;

		psci_ret = tftf_cpu_on(cpu_mpid, (uintptr_t) reissue_cpu_hotplug, 0);
		if (psci_ret != PSCI_E_SUCCESS)
			return TEST_RESULT_SKIPPED;

		/* Wait for the CPU to enter the test */
		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&entered_test[core_pos]);
	}

	return reissue_cpu_hotplug();
}

/*
 * @Test_Aim@ Hotplug request on a CPU that doesn't exist
 *
 * Such a hotplug request is expected to fail and the PSCI implementation is
 * expected to report that the parameters are invalid.
 */
test_result_t test_psci_cpu_hotplug_invalid_cpu(void)
{
	int psci_ret;

	/*
	 * 0xFFFFFFFF is an invalid MPID.
	 * Pass a valid entry point address to make sure that the call does not
	 * fail for the wrong reason.
	 */
	psci_ret = tftf_psci_cpu_on(0xFFFFFFFF,
		(uintptr_t) test_psci_cpu_hotplug_invalid_cpu, 0);

	return report_result(PSCI_E_INVALID_PARAMS, psci_ret);
}

/*
 * @Test_Aim@ Hotplug request on a CPU with invalid entrypoint address
 *
 * Such a hotplug request is expected to fail and the PSCI implementation is
 * expected to report that the entrypoint is invalid address for PSCI 1.0
 * onwards
 */
test_result_t test_psci_cpu_hotplug_invalid_ep(void)
{
	int psci_ret;
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_mpid, cpu_node;
	unsigned int psci_version;

	psci_version = tftf_get_psci_version();

	if (!(psci_version & PSCI_MAJOR_VER_MASK)) {
		tftf_testcase_printf(
			"PSCI Version is less then 1.0\n");
		return TEST_RESULT_SKIPPED;
	}

	/* Power on all CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already powered on */
		if (cpu_mpid == lead_mpid)
			continue;

		/*
		 * Here we need to bypass the framework and issue the SMC call
		 * directly from the test case itself as tftf_cpu_on calls SMC
		 * calls with hotplug as entry point. tftf_psci_cpu_on() is a
		 * simple wrapper over the SMC call.
		 *
		 * Entry point address argument can be any invalid address.
		 */

		psci_ret = tftf_psci_cpu_on(cpu_mpid, 0, 0);
		if (psci_ret != PSCI_E_INVALID_ADDRESS) {
			tftf_testcase_printf("CPU:0x%x Expected: %i Actual: %i\n",
						cpu_mpid,
						PSCI_E_INVALID_ADDRESS,
						psci_ret);
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}
