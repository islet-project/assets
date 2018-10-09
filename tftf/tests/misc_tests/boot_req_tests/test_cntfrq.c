/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mmio.h>
#include <plat_topology.h>
#include <power_management.h>
#include <psci.h>
#include <stdlib.h>
#include <test_helpers.h>
#include <tftf_lib.h>

static test_result_t cntfrq_check(void)
{
	u_register_t cntfrq_el0, ns_cntfrq;
	cntfrq_el0 = read_cntfrq_el0();

	ns_cntfrq = mmio_read_32(SYS_CNT_BASE1 + CNTBASEN_CNTFRQ);

	if (cntfrq_el0 != ns_cntfrq) {
		tftf_testcase_printf("CNTFRQ read from sys_reg = %llx and NS timer = %llx differs/n",
			(unsigned long long)cntfrq_el0,
			(unsigned long long)ns_cntfrq);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * The ARM ARM says that the cntfrq_el0, cntfrq memory mapped register and
 * the RO views in NS timer frames must all be initialized by the firmware.
 * (See I3.6.7 and D7.5.1 section in ARM ARM).
 * This tests the same on all the CPUs in the system.
 * Returns:
 *	TEST_RESULT_SUCCESS: if all the cntfrq values match
 *	TEST_RESULT_FAIL: if any of the cntfrq value mismatch
 */
test_result_t test_cntfrq_check(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_node, cpu_mpid;
	int rc;

	/* Bring every CPU online */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already on */
		if (cpu_mpid == lead_mpid)
			continue;

		rc = tftf_cpu_on(cpu_mpid,
				(uintptr_t) cntfrq_check,
				0);
		if (rc != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU 0x%x (%d)\n",
				cpu_mpid, rc);
			return TEST_RESULT_FAIL;
		}
	}

	rc = cntfrq_check();

	/* Wait for the CPUs to turn OFF */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);

		/* Wait for all non lead CPUs to turn OFF before returning */
		if (cpu_mpid == lead_mpid)
			continue;

		/* Wait for the target CPU to turn OFF */
		while (tftf_psci_affinity_info(cpu_mpid,
				MPIDR_AFFLVL0) != PSCI_STATE_OFF)
			;
	}

	return rc;
}
