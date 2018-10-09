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

#define TEST_ITERATIONS_COUNT 1000

static event_t cpu_has_entered_test[PLATFORM_CORE_COUNT];

/*
 * This function calls and validates the results of TSP operation.
 * It expects:
 *           - fn_identifier: SMC function identifier
 *           - arg1, arg2: data on which TSP performs operation
 *           - ret1, ret2: results expected after performing operation
 *                         on arg1 and arg2
 * Returns Success if return values of SMC operation are same as
 * expected else failure.
 */
static test_result_t validate_tsp_operations(uint64_t fn_identifier,
					   uint64_t arg1,
					   uint64_t arg2,
					   uint64_t ret1,
					   uint64_t ret2)
{
	smc_args tsp_svc_params = {fn_identifier, arg1, arg2};
	smc_ret_values tsp_result;

	tsp_result = tftf_smc(&tsp_svc_params);

	if (tsp_result.ret0) {
		tftf_testcase_printf("TSP operation 0x%x failed, error:0x%x\n",
					(unsigned int) fn_identifier,
					(unsigned int) tsp_result.ret0);
		return TEST_RESULT_FAIL;
	}

	if (tsp_result.ret1 != ret1 || tsp_result.ret2 != ret2) {
		tftf_testcase_printf("TSP function:0x%x returned wrong result:"
				     "got 0x%x 0x%x expected: 0x%x 0x%x\n",
						(unsigned int)fn_identifier,
						(unsigned int)tsp_result.ret1,
						(unsigned int)tsp_result.ret2,
						(unsigned int)ret1,
						(unsigned int)ret2);
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * This function issues SMC calls to trusted OS(TSP) to perform basic mathematical
 * operations supported by it and validates the result.
 */
static test_result_t issue_trustedos_service_calls(void)
{
	u_register_t mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);
	test_result_t ret;
	int i;

	/* Signal to the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_has_entered_test[core_pos]);

	for (i = 0; i < TEST_ITERATIONS_COUNT; i++) {
		/*
		 * TSP add function performs addition of argx to itself and
		 * returns the result in argx where x is 1, 2
		 */
		ret = validate_tsp_operations(TSP_FAST_FID(TSP_ADD), 4, 6, 8, 12);
		if (ret != TEST_RESULT_SUCCESS)
			return ret;

		/*
		 * TSP sub function performs substraction of argx to itself and
		 * returns the result in argx where x is 1, 2
		 */
		ret = validate_tsp_operations(TSP_FAST_FID(TSP_SUB), 4, 6, 0, 0);
		if (ret != TEST_RESULT_SUCCESS)
			return ret;

		/*
		 * TSP mul function performs multiplication of argx to itself and
		 * returns the result in argx where x is 1, 2
		 */
		ret = validate_tsp_operations(TSP_FAST_FID(TSP_MUL), 4, 6, 16, 36);
		if (ret != TEST_RESULT_SUCCESS)
			return ret;

		/*
		 * TSP div function performs division of argx to itself and
		 * returns the result in argx where x is 1, 2
		 */
		ret = validate_tsp_operations(TSP_FAST_FID(TSP_DIV), 4, 6, 1, 1);
		if (ret != TEST_RESULT_SUCCESS)
			return ret;
	}
	return ret;
}

/*
 * @Test_Aim@ Stress test the tsp functionality by issuing fast smc calls
 * to perform trusted OS operations on multiple CPUs
 * Returns Success/Failure/Skipped (if Trusted OS is absent or is not TSP)
 */
test_result_t test_tsp_fast_smc_operations(void)
{
	u_register_t lead_cpu;
	u_register_t cpu_mpid;
	int cpu_node;
	unsigned int core_pos;
	int ret;

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	lead_cpu = read_mpidr_el1() & MPID_MASK;

	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already on */
		if (cpu_mpid == lead_cpu)
			continue;

		ret = tftf_cpu_on(cpu_mpid,
				(uintptr_t) issue_trustedos_service_calls,
				0);
		if (ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("Failed to power on CPU 0x%llx (%i)\n",
					(unsigned long long) cpu_mpid, ret);
			return TEST_RESULT_FAIL;
		}
	}

	/* Wait for non-lead CPUs to enter the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_cpu)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_has_entered_test[core_pos]);
	}

	return issue_trustedos_service_calls();
}
