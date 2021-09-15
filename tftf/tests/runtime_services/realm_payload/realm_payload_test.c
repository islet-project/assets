/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_features.h>
#include <plat_topology.h>
#include <power_management.h>
#include <runtime_services/realm_payload/realm_payload_test.h>

static char bufferdelegate[GRANULE_SIZE] __aligned(GRANULE_SIZE);

/*
 * Overall test for realm payload in three sections:
 * 1. Single CPU version check: SMC call to realm payload to return
 * version information
 * 2. Multi CPU version check: SMC call to realm payload to return
 * version information from all CPU's in system
 * 3. Delegate and Undelegate Non-Secure granule via
 * SMC call to realm payload
 */

/*
 * Single CPU version check function
 */
test_result_t realm_version_single_cpu(void)
{

	u_register_t retrmm;

	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	retrmm = realm_version();

	tftf_testcase_printf("RMM version is: %lu.%lu\n",
			RMI_ABI_VERSION_GET_MAJOR(retrmm),
			RMI_ABI_VERSION_GET_MINOR(retrmm));

	return TEST_RESULT_SUCCESS;
}

/*
 * Multi CPU version check function in parallel.
 */
test_result_t realm_version_multi_cpu(void)
{

	u_register_t lead_mpid, target_mpid;
	int cpu_node;
	long long ret;

	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;

		if (lead_mpid == target_mpid) {
			continue;
		}

		ret = tftf_cpu_on(target_mpid,
			(uintptr_t)realm_multi_cpu_payload_test, 0);

		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
				(unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}

	}

	ret = realm_multi_cpu_payload_test();

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;

		if (lead_mpid == target_mpid) {
			continue;
		}

		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
				PSCI_STATE_OFF) {
			continue;
		}
	}

	return ret;
}

/*
 * Delegate and Undelegate Non Secure Granule
 */
test_result_t realm_delegate_undelegate(void)
{

	u_register_t retrmm;

	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	retrmm = realm_granule_delegate((u_register_t)bufferdelegate);
	if (retrmm != 0UL) {
		tftf_testcase_printf("Delegate operation returns fail, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}
	retrmm = realm_granule_undelegate((u_register_t)bufferdelegate);
	if (retrmm != 0UL) {
		tftf_testcase_printf("Undelegate operation returns fail, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}
	tftf_testcase_printf("Delegate and undelegate of buffer 0x%lx succeeded\n",
			(uintptr_t)bufferdelegate);

	return TEST_RESULT_SUCCESS;
}

test_result_t realm_multi_cpu_payload_test(void)
{
	u_register_t retrmm = realm_version();

	tftf_testcase_printf("Multi CPU RMM version on CPU %llx is: %lu.%lu\n",
			read_mpidr_el1() & MPID_MASK, RMI_ABI_VERSION_GET_MAJOR(retrmm),
			RMI_ABI_VERSION_GET_MINOR(retrmm));

	return TEST_RESULT_SUCCESS;
}
