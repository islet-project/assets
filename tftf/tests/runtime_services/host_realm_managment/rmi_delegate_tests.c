/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>

#include <arch_features.h>
#include <host_realm_helper.h>
#include <host_realm_mem_layout.h>
#include <host_shared_data.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include "rmi_spm_tests.h"
#include <test_helpers.h>



static test_result_t realm_multi_cpu_payload_test(void);
static test_result_t realm_multi_cpu_payload_del_undel(void);

/* Buffer to delegate and undelegate */
static char bufferdelegate[NUM_GRANULES * GRANULE_SIZE * PLATFORM_CORE_COUNT]
	__aligned(GRANULE_SIZE);
static char bufferstate[NUM_GRANULES * PLATFORM_CORE_COUNT];

/*
 * Overall test for realm payload in three sections:
 * 1. Single CPU version check: SMC call to realm payload to return
 * version information
 * 2. Multi CPU version check: SMC call to realm payload to return
 * version information from all CPU's in system
 * 3. Delegate and Undelegate Non-Secure granule via
 * SMC call to realm payload
 * 4. Multi CPU delegation where random assignment of states
 * (realm, non-secure)is assigned to a set of granules.
 * Each CPU is given a number of granules to delegate in
 * parallel with the other CPU's
 * 5. Fail testing of delegation parameters such as
 * attempting to perform a delegation on the same granule
 * twice and then testing a misaligned address
 */

test_result_t init_buffer_del(void)
{
	u_register_t retrmm;

	for (uint32_t i = 0; i < (NUM_GRANULES * PLATFORM_CORE_COUNT) ; i++) {
		if ((rand() % 2) == 0) {
			retrmm = rmi_granule_delegate(
					(u_register_t)&bufferdelegate[i * GRANULE_SIZE]);
			bufferstate[i] = B_DELEGATED;
			if (retrmm != 0UL) {
				tftf_testcase_printf("Delegate operation returns fail, %lx\n",
						retrmm);
				return TEST_RESULT_FAIL;
			}
		} else {
			bufferstate[i] = B_UNDELEGATED;
		}
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * Single CPU version check function
 */
test_result_t realm_version_single_cpu(void)
{
	u_register_t retrmm;

	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	retrmm = rmi_version();

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

	retrmm = rmi_granule_delegate((u_register_t)bufferdelegate);
	if (retrmm != 0UL) {
		tftf_testcase_printf("Delegate operation returns fail, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}
	retrmm = rmi_granule_undelegate((u_register_t)bufferdelegate);
	if (retrmm != 0UL) {
		tftf_testcase_printf("Undelegate operation returns fail, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}
	tftf_testcase_printf("Delegate and undelegate of buffer 0x%lx succeeded\n",
			(uintptr_t)bufferdelegate);

	return TEST_RESULT_SUCCESS;
}

static test_result_t realm_multi_cpu_payload_test(void)
{
	u_register_t retrmm = rmi_version();

	tftf_testcase_printf("Multi CPU RMM version on CPU %llx is: %lu.%lu\n",
			(long long)read_mpidr_el1() & MPID_MASK, RMI_ABI_VERSION_GET_MAJOR(retrmm),
			RMI_ABI_VERSION_GET_MINOR(retrmm));

	return TEST_RESULT_SUCCESS;
}

/*
 * Select all CPU's to randomly delegate/undelegate
 * granule pages to stress the delegate mechanism
 */
test_result_t realm_delundel_multi_cpu(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node;
	long long ret;
	u_register_t retrmm;

	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	if (init_buffer_del() == TEST_RESULT_FAIL) {
		return TEST_RESULT_FAIL;
	}

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;

		if (lead_mpid == target_mpid) {
			continue;
		}

		ret = tftf_cpu_on(target_mpid,
			(uintptr_t)realm_multi_cpu_payload_del_undel, 0);

		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
				(unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}

	}

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

	/*
	 * Cleanup to set all granules back to undelegated
	 */

	for (uint32_t i = 0; i < (NUM_GRANULES * PLATFORM_CORE_COUNT) ; i++) {
		if (bufferstate[i] == B_DELEGATED) {
			retrmm = rmi_granule_undelegate(
					(u_register_t)&bufferdelegate[i * GRANULE_SIZE]);
			bufferstate[i] = B_UNDELEGATED;
			if (retrmm != 0UL) {
				tftf_testcase_printf("Delegate operation returns fail, %lx\n",
						retrmm);
				return TEST_RESULT_FAIL;
			}
		}
	}

	ret = TEST_RESULT_SUCCESS;
	return ret;
}

/*
 * Multi CPU testing of delegate and undelegate of granules
 * The granules are first randomly initialized to either realm or non secure
 * using the function init_buffer_del and then the function below
 * assigns NUM_GRANULES to each CPU for delegation or undelgation
 * depending upon the initial state
 */
static test_result_t realm_multi_cpu_payload_del_undel(void)
{
	u_register_t retrmm;
	unsigned int cpu_node;

	cpu_node = platform_get_core_pos(read_mpidr_el1() & MPID_MASK);

	for (uint32_t i = 0; i < NUM_GRANULES; i++) {
		if (bufferstate[((cpu_node * NUM_GRANULES) + i)] == B_UNDELEGATED) {
			retrmm = rmi_granule_delegate((u_register_t)
					&bufferdelegate[((cpu_node * NUM_GRANULES) + i) * GRANULE_SIZE]);
			bufferstate[((cpu_node * NUM_GRANULES) + i)] = B_DELEGATED;
		} else {
			retrmm = rmi_granule_undelegate((u_register_t)
					&bufferdelegate[((cpu_node * NUM_GRANULES) + i) * GRANULE_SIZE]);
			bufferstate[((cpu_node * NUM_GRANULES) + i)] = B_UNDELEGATED;
		}
		if (retrmm != 0UL) {
			tftf_testcase_printf("Delegate operation returns fail, %lx\n", retrmm);
			return TEST_RESULT_FAIL;
		}
	}
	return TEST_RESULT_SUCCESS;
}

/*Fail testing of delegation process. The first is an error expected
 * for processing the same granule twice and the second is submission of
 * a misaligned address
 */

test_result_t realm_fail_del(void)
{
	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	u_register_t retrmm;

	retrmm = rmi_granule_delegate((u_register_t)&bufferdelegate[0]);

	if (retrmm != 0UL) {
		tftf_testcase_printf
			("Delegate operation does not pass as expected for double delegation, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}

	retrmm = rmi_granule_delegate((u_register_t)&bufferdelegate[0]);

	if (retrmm == 0UL) {
		tftf_testcase_printf
			("Delegate operation does not fail as expected for double delegation, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}

	retrmm = rmi_granule_undelegate((u_register_t)&bufferdelegate[1]);

	if (retrmm == 0UL) {
		tftf_testcase_printf
			("Delegate operation does not return fail for misaligned address, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}

	retrmm = rmi_granule_undelegate((u_register_t)&bufferdelegate[0]);

	if (retrmm != 0UL) {
		tftf_testcase_printf
			("Delegate operation returns fail for cleanup, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
