/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psci.h>
#include <smccc.h>
#include <tftf_lib.h>
#include <test_helpers.h>
#include <power_management.h>

/* Generic function to call System OFF SMC */
static test_result_t test_cpu_system_off(void)
{
	u_register_t curr_mpid = read_mpidr_el1() & MPID_MASK;
	u_register_t mpid;
	unsigned int cpu_node;
	smc_args args = { SMC_PSCI_SYSTEM_OFF };

	/* Wait for all other CPU's to turn off */
	for_each_cpu(cpu_node) {
		mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip current CPU */
		if (mpid == curr_mpid)
			continue;

		while (tftf_psci_affinity_info(mpid,
			MPIDR_AFFLVL0) != PSCI_STATE_OFF) {
		}
	}

	INFO("System off from CPU MPID 0x%lx\n", curr_mpid);
	tftf_notify_reboot();
	tftf_smc(&args);

	/* The PSCI SYSTEM_OFF call is not supposed to return */
	tftf_testcase_printf("System didn't shutdown properly\n");
	return TEST_RESULT_FAIL;
}

/*
 * @Test_Aim@ Validate the SYSTEM_OFF call.
 * Test SUCCESS in case of system shutdown.
 * Test FAIL in case of execution not terminated.
 */
test_result_t test_system_off(void)
{
	if (tftf_is_rebooted() == 1) {
		/* Successfully resumed from SYSTEM_OFF */
		return TEST_RESULT_SUCCESS;
	}

	return test_cpu_system_off();
}

/*
 * @Test_Aim@ Validate the SYSTEM_OFF call on CPU other than lead CPU
 * Test SUCCESS in case of system shutdown.
 * Test FAIL in case of execution not terminated.
 */
test_result_t test_system_off_cpu_other_than_lead(void)
{
	u_register_t lead_mpid = read_mpidr_el1() & MPID_MASK;
	u_register_t cpu_mpid;
	int psci_ret;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	if (tftf_is_rebooted() == 1) {
		/* Successfully resumed from SYSTEM_OFF */
		return TEST_RESULT_SUCCESS;
	}

	/* Power ON another CPU, other than the lead CPU */
	cpu_mpid = tftf_find_random_cpu_other_than(lead_mpid);
	VERBOSE("CPU to be turned on MPID: 0x%lx\n", cpu_mpid);
	psci_ret = tftf_cpu_on(cpu_mpid,
			   (uintptr_t)test_cpu_system_off,
			   0);

	if (psci_ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to power on CPU 0x%lx (%d)\n",
				cpu_mpid, psci_ret);
		return TEST_RESULT_FAIL;
	}

	/* Power down the lead CPU */
	INFO("Lead CPU to be turned off MPID: 0x%lx\n", lead_mpid);
	tftf_cpu_off();

	/* Test should not reach here */
	return TEST_RESULT_FAIL;
}
