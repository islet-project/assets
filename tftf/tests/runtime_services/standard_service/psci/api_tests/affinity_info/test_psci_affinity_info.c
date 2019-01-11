/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <drivers/arm/arm_gic.h>
#include <events.h>
#include <irq.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <test_helpers.h>
#include <tftf_lib.h>

/* Special value used to terminate the array of expected return values */
#define END_OF_EXPECTED_VALUE	0xDEADBEEF

/* TODO: Remove assumption that affinity levels always map to power levels. */
#define MPIDR_CLUSTER_ID(mpid)	MPIDR_AFF_ID(mpid, 1)
#define MPIDR_CPU_ID(mpid)	MPIDR_AFF_ID(mpid, 0)

/*
 * Event used by test test_affinity_info_level0_powerdown() to synchronise
 * CPUs
 */
static event_t cpu_about_to_suspend;
static unsigned int psci_version;

/*
 * `expected_values` should contain an array of expected return values
 * terminated by `END_OF_EXPECTED_VALUE`. If 'actual_value' exists in
 * one of 'expected_values' then return a test success.
 * Otherwise, print an error message in the test report and report a test
 * failure.
 */
static test_result_t get_test_result(const int *expected_values, int actual_value)
{
	const int *expected_val_list;

	expected_val_list = expected_values;
	while (*expected_val_list != END_OF_EXPECTED_VALUE) {
		if (*expected_val_list == actual_value)
			return TEST_RESULT_SUCCESS;
		expected_val_list++;
	}

	expected_val_list = expected_values;
	tftf_testcase_printf("Unexpected return value: %i Expected values are:",
						actual_value);
	while (*expected_val_list != END_OF_EXPECTED_VALUE) {
		tftf_testcase_printf("%i ", *expected_val_list);
		expected_val_list++;
	}
	tftf_testcase_printf("\n");

	return TEST_RESULT_FAIL;
}

/*
 * @Test_Aim@ Test PSCI AFFINITY_INFO targeted at affinity level 0 on online CPU
 *
 * Call PSCI AFFINITY_INFO targeted at affinity level 0 on lead CPU.
 * Expect the PSCI implementation to report that the affinity instance is on.
 */
test_result_t test_affinity_info_level0_on(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	int32_t aff_info;
	int expected_values[] = {PSCI_STATE_ON, END_OF_EXPECTED_VALUE};

	aff_info = tftf_psci_affinity_info(mpid, MPIDR_AFFLVL0);
	return get_test_result(expected_values, aff_info);
}

/*
 * @Test_Aim@ Test PSCI AFFINITY_INFO targeted at affinity level 0 on offline CPU
 *
 * Call PSCI AFFINITY_INFO targeted at affinity level 0 on all non-lead CPUs.
 * Expect the PSCI implementation to report that the affinity instances are off.
 *
 * This test needs 2 CPUs to run. It will be skipped on a single core platform.
 */
test_result_t test_affinity_info_level0_off(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int target_mpid, target_node;
	int32_t aff_info;
	test_result_t ret = TEST_RESULT_SUCCESS;
	int expected_values[] = {PSCI_STATE_OFF, END_OF_EXPECTED_VALUE};

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU, as it is powered on */
		if (target_mpid == lead_mpid)
			continue;

		aff_info = tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0);
		if (get_test_result(expected_values, aff_info)
			== TEST_RESULT_FAIL) {
			ret = TEST_RESULT_FAIL;
		}
	}

	return ret;
}

/*
 * @Test_Aim@ Test PSCI AFFINITY_INFO targeted at affinity level 1 on online cluster
 *
 * Call PSCI AFFINITY_INFO targeted at affinity level 1 on the lead cluster
 * (i.e. the cluster to which the lead CPU belongs to).
 * PSCI implementation prior to PSCI 1.0 needs to report that the cluster is on
 * and others can also return INVALID_PARAMETERS.
 */
test_result_t test_affinity_info_level1_on(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int target_mpid;
	int32_t aff_info;
	int expected_values[3];

	/*
	 * Minimum version of PSCI is 0.2, uses this info to decide if
	 * tftf_get_psci_version() needs to be called or not.
	 */
	if (!psci_version)
		psci_version = tftf_get_psci_version();

	/*
	 * From PSCI version 1.0 onwards, Trusted Firmware-A may or may not
	 * track affinity levels greater than zero.
	 */
	if (!(psci_version & PSCI_MAJOR_VER_MASK)) {
		expected_values[0] = PSCI_STATE_ON;
		expected_values[1] = END_OF_EXPECTED_VALUE;
	} else {
		expected_values[0] = PSCI_STATE_ON;
		expected_values[1] = PSCI_E_INVALID_PARAMS;
		expected_values[2] = END_OF_EXPECTED_VALUE;
	}

	/*
	 * Build an MPID corresponding to the lead cluster. Set the affinity
	 * level0 bits to some arbitrary value that doesn't correspond to any
	 * CPU on the platform. The PSCI implementation should ignore the
	 * affinity 0 field.
	 */
	target_mpid = (lead_mpid & MPIDR_CLUSTER_MASK) | 0xE1;
	aff_info = tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL1);
	return get_test_result(expected_values, aff_info);
}

/*
 * @Test_Aim@ Test PSCI AFFINITY_INFO targeted at affinity level 1 on offline cluster
 *
 * Call PSCI AFFINITY_INFO targeted at affinity level 1 on a non-lead cluster
 * (i.e. another cluster than the one to which the lead CPU belongs to).
 * PSCI implementation prior to PSCI 1.0 needs to report that the cluster is OFF
 * and others can also return INVALID_PARAMETERS.
 *
 * This test needs 2 clusters to run. It will be skipped on a single cluster
 * platform.
 */
test_result_t test_affinity_info_level1_off(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int target_mpid;
	int32_t aff_info;
	unsigned int cluster_id;
	int expected_values[3];

	SKIP_TEST_IF_LESS_THAN_N_CLUSTERS(2);

	for (cluster_id = 0;
	     cluster_id < tftf_get_total_clusters_count();
	     ++cluster_id) {
		if (cluster_id != MPIDR_CLUSTER_ID(lead_mpid))
			break;
	}
	assert(cluster_id != tftf_get_total_clusters_count());


	/*
	 * Minimum version of PSCI is 0.2, uses this info to decide if
	 * tftf_get_psci_version() needs to be called or not.
	 */
	if (!psci_version)
		psci_version = tftf_get_psci_version();

	/*
	 * From PSCI version 1.0 onwards, Trusted Firmware-A may or may not
	 * track affinity levels greater than zero.
	 */
	if (!(psci_version & PSCI_MAJOR_VER_MASK)) {
		expected_values[0] = PSCI_STATE_OFF;
		expected_values[1] = END_OF_EXPECTED_VALUE;
	} else {
		expected_values[0] = PSCI_STATE_OFF;
		expected_values[1] = PSCI_E_INVALID_PARAMS;
		expected_values[2] = END_OF_EXPECTED_VALUE;
	}

	/*
	 * Build an MPID corresponding to a non-lead cluster. Set the affinity
	 * level0 bits to some arbitrary value that doesn't correspond to any
	 * CPU on the platform. The PSCI implementation should ignore the
	 * affinity 0 field.
	 */
	target_mpid = make_mpid(cluster_id, 0xE1);
	aff_info = tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL1);
	return get_test_result(expected_values, aff_info);
}

/*
 * @Test_Aim@ Test PSCI AFFINITY_INFO targeted at affinity level 2
 *
 * For PSCI implementations prior to PSCI 1.0 , the expected return value
 * depends on the the maximum affinity level that the power management
 * operations can apply to on this platform.
 *  - If the platform doesn't have an affinity level 2 then expect the PSCI
 *    implementation to report that it received invalid parameters.
 *  - If affinity level 2 exists then expect the PSCI implementation to report
 *    that the affinity instance is on.
 *
 * From PSCI 1.0 onwards, it can also return either INVALID_PARMETERS
 */
test_result_t test_affinity_info_level2(void)
{
	int expected_values[3];
	unsigned int target_mpid;
	int32_t aff_info;

	/*
	 * Minimum version of PSCI is 0.2, uses this info to decide if
	 * tftf_get_psci_version() needs to be called or not.
	 */
	if (!psci_version)
		psci_version = tftf_get_psci_version();

	expected_values[0] = (PLATFORM_MAX_AFFLVL >= 2)
		? PSCI_STATE_ON
		: PSCI_E_INVALID_PARAMS;

	/*
	 * From PSCI version 1.0 onwards, Trusted Firmware-A may or may not
	 * track affinity levels greater than zero.
	 */
	if (!(psci_version & PSCI_MAJOR_VER_MASK)) {
		expected_values[1] = END_OF_EXPECTED_VALUE;
	} else {
		expected_values[1] = PSCI_E_INVALID_PARAMS;
		expected_values[2] = END_OF_EXPECTED_VALUE;
	}

	/*
	 * Build an MPID corresponding to the lead affinity instance at level 2.
	 * Set the affinity level0 & level1 bits to some arbitrary values that
	 * don't correspond to any affinity instance on the platform. The PSCI
	 * implementation should ignore the affinity 0 & 1 fields.
	 */
	target_mpid = read_mpidr_el1() & (MPIDR_AFFLVL_MASK << MPIDR_AFF_SHIFT(2));
	target_mpid |= 0xAB << MPIDR_AFF1_SHIFT;
	target_mpid |= 0xE1 << MPIDR_AFF0_SHIFT;

	aff_info = tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL2);
	return get_test_result(expected_values, aff_info);
}

/*
 * @Test_Aim@ Test PSCI AFFINITY_INFO targeted at affinity level 3
 *
 * For PSCI implementations prior to PSCI 1.0 , the expected return value
 * depends on the maximum affinity level that the power management
 * operations can apply to on this platform.
 *  - If the platform doesn't have an affinity level 3 then expect the PSCI
 *    implementation to report that it received invalid parameters.
 *  - If affinity level 3 exists then expect the PSCI implementation to report
 *    that the affinity instance is on.
 *
 * From PSCI 1.0 onwards, it can also return INVALID_PARMETERS
 */
test_result_t test_affinity_info_level3(void)
{
#ifndef AARCH32
	int expected_values[3];
	uint64_t target_mpid;
	int32_t aff_info;

	/*
	 * Minimum version of PSCI is 0.2, uses this info to decide if
	 * tftf_get_psci_version() needs to be called or not.
	 */
	if (!psci_version)
		psci_version = tftf_get_psci_version();

	expected_values[0] = (PLATFORM_MAX_AFFLVL == 3)
		? PSCI_STATE_ON
		: PSCI_E_INVALID_PARAMS;

	/*
	 * From PSCI version 1.0 onwards, Trusted Firmware-A may or may not
	 * track affinity levels greater than zero.
	 */
	if (!(psci_version & PSCI_MAJOR_VER_MASK)) {
		expected_values[1] = END_OF_EXPECTED_VALUE;
	} else {
		expected_values[1] = PSCI_E_INVALID_PARAMS;
		expected_values[2] = END_OF_EXPECTED_VALUE;
	}

	/*
	 * Build an MPID corresponding to the lead affinity instance at level 3.
	 * Set the affinity level0/level1/level2 bits to some arbitrary values
	 * that don't correspond to any affinity instance on the platform. The
	 * PSCI implementation should ignore the affinity 0, 1 & 2 fields.
	 */
	target_mpid = read_mpidr_el1();
	target_mpid &= ((uint64_t) MPIDR_AFFLVL_MASK) << MPIDR_AFF_SHIFT(3);
	target_mpid |= 0xD2 << MPIDR_AFF2_SHIFT;
	target_mpid |= 0xAB << MPIDR_AFF1_SHIFT;
	target_mpid |= 0xE1 << MPIDR_AFF0_SHIFT;

	aff_info = tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL3);
	return get_test_result(expected_values, aff_info);
#else
	return TEST_RESULT_SKIPPED;
#endif
}

/*
 * Suspend to powerdown the calling CPU.
 *
 * 1) Enable SGI #0. This SGI will be sent by the lead CPU to wake this CPU.
 * 2) Suspend the CPU.
 * 3) Report success/failure of the suspend operation.
 */
static test_result_t suspend_to_powerdown(void)
{
	uint32_t power_state, stateid;
	int psci_ret, expected_return_val;

	/*
	 * Enable reception of SGI 0 on the calling CPU.
	 * SGI 0 will serve as the wake-up event to come out of suspend.
	 */
	tftf_irq_enable(IRQ_NS_SGI_0, GIC_HIGHEST_NS_PRIORITY);

	expected_return_val = tftf_psci_make_composite_state_id(
			PSTATE_AFF_LVL_0, PSTATE_TYPE_POWERDOWN, &stateid);

	/* Need at least 1 power down state defined at level 0 */
	if (expected_return_val != PSCI_E_SUCCESS)
		return TEST_RESULT_SKIPPED;

	/*
	 * Suspend the calling CPU to the desired affinity level and power state
	 */
	power_state = tftf_make_psci_pstate(PSTATE_AFF_LVL_0,
					  PSTATE_TYPE_POWERDOWN,
					  stateid);

	/*
	 * Notify the lead CPU that the calling CPU is about to suspend itself
	 */
	tftf_send_event(&cpu_about_to_suspend);

	psci_ret = tftf_cpu_suspend(power_state);

	tftf_irq_disable(IRQ_NS_SGI_0);

	if (psci_ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to suspend (%i)\n", psci_ret);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Test PSCI AFFINITY_INFO targeted at affinity level 0 on a
 *            suspended CPU
 *
 * A CPU that has been physically powered down as a result of a call to
 * CPU_SUSPEND must be reported as ON by the AFFINITY_INFO call. This test
 * aims at verifying this behaviour.
 *
 * This test needs 2 CPUs to run. It will be skipped on a single core platform.
 * It will also be skipped if an error is encountered during the bring-up of the
 * non-lead CPU.
 */
test_result_t test_affinity_info_level0_powerdown(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int target_mpid, target_core_pos;
	int psci_ret;
	int32_t aff_info;
	test_result_t ret;
	int expected_values[] = {PSCI_STATE_ON, END_OF_EXPECTED_VALUE};

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	/*
	 * Preparation step:
	 * Find another CPU than the lead CPU and power it on.
	 */
	target_mpid = tftf_find_any_cpu_other_than(lead_mpid);
	assert(target_mpid != INVALID_MPID);
	target_core_pos = platform_get_core_pos(target_mpid);

	psci_ret = tftf_cpu_on(target_mpid, (uintptr_t) suspend_to_powerdown, 0);
	if (psci_ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to power on CPU 0x%x (%d)\n",
				     target_mpid, psci_ret);
		return TEST_RESULT_SKIPPED;
	}

	/* Wait for the other CPU to initiate the suspend operation */
	tftf_wait_for_event(&cpu_about_to_suspend);

	/* Wait a bit for the CPU to really enter suspend state */
	waitms(PLAT_SUSPEND_ENTRY_TIME);

	/* Request status of the non-lead CPU while it is suspended */
	aff_info = tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0);
	ret = get_test_result(expected_values, aff_info);

	/* Wake up non-lead CPU */
	tftf_send_sgi(IRQ_NS_SGI_0, target_core_pos);

	return ret;
}
