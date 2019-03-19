/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <events.h>
#include <mm_svc.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <secure_partition.h>
#include <smccc.h>
#include <spm_svc.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <xlat_tables_v2.h>

static event_t cpu_has_finished_test[PLATFORM_CORE_COUNT];

/* Test routine for test_secure_partition_secondary_cores_seq() */
static test_result_t test_secure_partition_secondary_cores_seq_fn(void)
{
	test_result_t result = TEST_RESULT_SUCCESS;
	u_register_t cpu_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(cpu_mpid);

	secure_partition_request_info_t *sps_request =
		create_sps_request(SPS_CHECK_ALIVE, NULL, 0);

	INFO("Sending MM_COMMUNICATE_AARCH64 from CPU %u\n",
	     platform_get_core_pos(read_mpidr_el1() & MPID_MASK));

	smc_args mm_communicate_smc = {
		MM_COMMUNICATE_AARCH64,
		0,
		(u_register_t) sps_request,
		0
	};

	smc_ret_values smc_ret = tftf_smc(&mm_communicate_smc);

	if ((uint32_t)smc_ret.ret0 != 0) {
		tftf_testcase_printf("Cactus returned: 0x%x\n",
				     (uint32_t)smc_ret.ret0);

		result = TEST_RESULT_FAIL;
	}

	tftf_send_event(&cpu_has_finished_test[core_pos]);

	return result;
}

/*
 * @Test_Aim@ This tests that secondary CPUs can access SPM services
 * sequentially.
 */
test_result_t test_secure_partition_secondary_cores_seq(void)
{
	int psci_ret;
	u_register_t lead_mpid, cpu_mpid;
	unsigned int cpu_node, core_pos;
	test_result_t result = TEST_RESULT_SUCCESS;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	SKIP_TEST_IF_MM_VERSION_LESS_THAN(1, 0);

	VERBOSE("Mapping NS<->SP shared buffer\n");

	int rc = mmap_add_dynamic_region(ARM_SECURE_SERVICE_BUFFER_BASE,
					 ARM_SECURE_SERVICE_BUFFER_BASE,
					 ARM_SECURE_SERVICE_BUFFER_SIZE,
					 MT_MEMORY | MT_RW | MT_NS);
	if (rc != 0) {
		tftf_testcase_printf("%d: mmap_add_dynamic_region() = %d\n",
				     __LINE__, rc);
		result = TEST_RESULT_FAIL;
		goto exit;
	}

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	INFO("Lead CPU is CPU %u\n", platform_get_core_pos(lead_mpid));

	if (test_secure_partition_secondary_cores_seq_fn() != TEST_RESULT_SUCCESS) {
		result = TEST_RESULT_FAIL;
		goto exit_unmap;
	}

	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU, we have already tested it */
		if (cpu_mpid == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(cpu_mpid);

		tftf_init_event(&cpu_has_finished_test[core_pos]);

		VERBOSE("Powering on CPU %u\n", core_pos);

		psci_ret = tftf_cpu_on(cpu_mpid,
			(uintptr_t)test_secure_partition_secondary_cores_seq_fn, 0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU %d (rc = %d)\n",
				core_pos, psci_ret);
			result = TEST_RESULT_FAIL;
			goto exit_unmap;
		}

		tftf_wait_for_event(&cpu_has_finished_test[core_pos]);
	}

exit_unmap:
	VERBOSE("Unmapping NS<->SP shared buffer\n");

	mmap_remove_dynamic_region(ARM_SECURE_SERVICE_BUFFER_BASE,
				   ARM_SECURE_SERVICE_BUFFER_SIZE);

exit:
	return result;
}

/******************************************************************************/

static event_t cpu_can_start_test[PLATFORM_CORE_COUNT];

/* Test routine for test_secure_partition_secondary_core() */
static test_result_t test_secure_partition_secondary_cores_sim_fn(void)
{
	test_result_t result = TEST_RESULT_SUCCESS;
	u_register_t cpu_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(cpu_mpid);

	secure_partition_request_info_t *sps_request =
		create_sps_request(SPS_CHECK_ALIVE, NULL, 0);

	smc_args mm_communicate_smc = {
		MM_COMMUNICATE_AARCH64,
		0,
		(u_register_t) sps_request,
		0
	};

	tftf_wait_for_event(&cpu_can_start_test[core_pos]);

	/*
	 * Invoke SMCs for some time to make sure that all CPUs are doing it at
	 * the same time during the test.
	 */
	for (int i = 0; i < 100; i++) {
		smc_ret_values smc_ret = tftf_smc(&mm_communicate_smc);

		if ((uint32_t)smc_ret.ret0 != 0) {
			tftf_testcase_printf("Cactus returned 0x%x at CPU %d\n",
					     (uint32_t)smc_ret.ret0, core_pos);
			result = TEST_RESULT_FAIL;
			break;
		}
	}

	tftf_send_event(&cpu_has_finished_test[core_pos]);

	return result;
}

/*
 * @Test_Aim@ This tests that secondary CPUs can access SPM services
 * simultaneously.
 */
test_result_t test_secure_partition_secondary_cores_sim(void)
{
	int psci_ret;
	u_register_t lead_mpid, cpu_mpid;
	unsigned int cpu_node, core_pos;
	test_result_t result = TEST_RESULT_SUCCESS;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	SKIP_TEST_IF_MM_VERSION_LESS_THAN(1, 0);

	VERBOSE("Mapping NS<->SP shared buffer\n");

	int rc = mmap_add_dynamic_region(ARM_SECURE_SERVICE_BUFFER_BASE,
					 ARM_SECURE_SERVICE_BUFFER_BASE,
					 ARM_SECURE_SERVICE_BUFFER_SIZE,
					 MT_MEMORY | MT_RW | MT_NS);
	if (rc != 0) {
		tftf_testcase_printf("%d: mmap_add_dynamic_region() = %d\n",
				     __LINE__, rc);
		result = TEST_RESULT_FAIL;
		goto exit;
	}

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	INFO("Lead CPU is CPU %u\n", platform_get_core_pos(lead_mpid));

	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_init_event(&cpu_can_start_test[core_pos]);
	}

	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already powered on */
		if (cpu_mpid == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(cpu_mpid);

		VERBOSE("Powering on CPU %u\n", core_pos);

		psci_ret = tftf_cpu_on(cpu_mpid,
				       (uintptr_t)test_secure_partition_secondary_cores_sim_fn, 0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU %d (rc = %d)\n",
				core_pos, psci_ret);
			result = TEST_RESULT_FAIL;
			goto exit_unmap;
		}
	}

	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_send_event(&cpu_can_start_test[core_pos]);
	}

	result = test_secure_partition_secondary_cores_sim_fn();

	/* Wait until all CPUs have finished to unmap the NS<->SP buffer */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_has_finished_test[core_pos]);
	}
exit_unmap:
	VERBOSE("Unmapping NS<->SP shared buffer\n");

	mmap_remove_dynamic_region(ARM_SECURE_SERVICE_BUFFER_BASE,
				   ARM_SECURE_SERVICE_BUFFER_SIZE);
exit:
	return result;
}
