/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <cactus_def.h>
#include <debug.h>
#include <events.h>
#include <ivy_def.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <smccc.h>
#include <stdbool.h>
#include <spci_helpers.h>
#include <spci_svc.h>
#include <test_helpers.h>
#include <tftf_lib.h>

#define TEST_NUM_ITERATIONS	1000U

test_result_t test_spci_non_blocking_fn(void)
{
	int ret;
	u_register_t ret1;
	uint16_t handle_cactus, handle_ivy;
	uint32_t token_cactus, token_ivy;
	test_result_t result = TEST_RESULT_SUCCESS;

	/* Open handles. */

	ret = spci_service_handle_open(TFTF_SPCI_CLIENT_ID, &handle_cactus,
				       CACTUS_SERVICE1_UUID);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to return a valid handle. Returned: 0x%x\n",
				     __LINE__, (uint32_t)ret);
		result = TEST_RESULT_FAIL;
		goto exit_none;
	}

	ret = spci_service_handle_open(TFTF_SPCI_CLIENT_ID, &handle_ivy,
				       IVY_SERVICE1_UUID);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to return a valid handle. Returned: 0x%x\n",
				     __LINE__, (uint32_t)ret);
		result = TEST_RESULT_FAIL;
		goto exit_cactus;
	}

	/* Request services. */

	for (unsigned int i = 0U; i < TEST_NUM_ITERATIONS; i++) {

		bool exit_loop = false;

		/* Send request to Cactus */

		ret = spci_service_request_start(CACTUS_GET_MAGIC,
						 0, 0, 0, 0, 0,
						 TFTF_SPCI_CLIENT_ID,
						 handle_cactus,
						 &token_cactus);
		if (ret != SPCI_SUCCESS) {
			tftf_testcase_printf("%d: SPM should have returned SPCI_SUCCESS. Returned: 0x%x\n",
					     __LINE__, (uint32_t)ret);
			result = TEST_RESULT_FAIL;
			exit_loop = true;
		}

		/* Send request to Ivy */

		ret = spci_service_request_start(IVY_GET_MAGIC,
						 0, 0, 0, 0, 0,
						 TFTF_SPCI_CLIENT_ID,
						 handle_ivy,
						 &token_ivy);
		if (ret != SPCI_SUCCESS) {
			tftf_testcase_printf("%d: SPM should have returned SPCI_SUCCESS. Returned: 0x%x\n",
					     __LINE__, (uint32_t)ret);
			result = TEST_RESULT_FAIL;
			exit_loop = true;
		}

		/* Get response from Ivy */

		do {
			ret = spci_service_request_resume(TFTF_SPCI_CLIENT_ID,
							  handle_ivy,
							  token_ivy,
							  &ret1, NULL, NULL);
		} while (ret == SPCI_QUEUED);

		if ((ret != SPCI_SUCCESS) || (ret1 != IVY_MAGIC_NUMBER)) {
			tftf_testcase_printf("%d: Ivy returned 0x%x 0x%lx\n",
					__LINE__, (uint32_t)ret, ret1);
			result = TEST_RESULT_FAIL;
			exit_loop = true;
		}

		/* Get response from Cactus */

		do {
			ret = spci_service_request_resume(TFTF_SPCI_CLIENT_ID,
							  handle_cactus,
							  token_cactus,
							  &ret1, NULL, NULL);
		} while (ret == SPCI_QUEUED);

		if ((ret != SPCI_SUCCESS) || (ret1 != CACTUS_MAGIC_NUMBER)) {
			tftf_testcase_printf("%d: Cactus returned 0x%x 0x%lx\n",
					__LINE__, (uint32_t)ret, ret1);
			result = TEST_RESULT_FAIL;
			exit_loop = true;
		}

		/* If there has been an error, exit early. */
		if (exit_loop) {
			break;
		}
	}

	/* Close handles. */

	ret = spci_service_handle_close(TFTF_SPCI_CLIENT_ID, handle_ivy);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to close the handle. Returned: 0x%x\n",
				     __LINE__, (uint32_t)ret);
		result = TEST_RESULT_FAIL;
	}

exit_cactus:
	ret = spci_service_handle_close(TFTF_SPCI_CLIENT_ID, handle_cactus);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to close the handle. Returned: 0x%x\n",
				     __LINE__, (uint32_t)ret);
		result = TEST_RESULT_FAIL;
	}

	/* All tests passed. */
exit_none:
	return result;
}

/*
 * @Test_Aim@ This tests opens a Secure Service handle and performs many simple
 * non-blocking requests to Cactus and Ivy.
 */
test_result_t test_spci_request(void)
{
	SKIP_TEST_IF_SPCI_VERSION_LESS_THAN(0, 1);

	return test_spci_non_blocking_fn();
}

/******************************************************************************/

static event_t cpu_has_entered_test[PLATFORM_CORE_COUNT];

static test_result_t test_spci_non_blocking_multicore_fn(void)
{
	u_register_t cpu_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(cpu_mpid);

	tftf_send_event(&cpu_has_entered_test[core_pos]);

	return test_spci_non_blocking_fn();
}

/*
 * @Test_Aim@ This tests opens a Secure Service handle and performs many simple
 * non-blocking requests to Cactus and Ivy from multiple cores
 */
test_result_t test_spci_request_multicore(void)
{
	unsigned int cpu_node, core_pos;
	int psci_ret;
	u_register_t cpu_mpid;
	u_register_t lead_mpid = read_mpidr_el1() & MPID_MASK;

	SKIP_TEST_IF_SPCI_VERSION_LESS_THAN(0, 1);

	for (int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		tftf_init_event(&cpu_has_entered_test[i]);
	}

	/* Power on all CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already powered on */
		if (cpu_mpid == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(cpu_mpid);

		psci_ret = tftf_cpu_on(cpu_mpid,
				(uintptr_t)test_spci_non_blocking_multicore_fn, 0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU %d (rc = %d)\n",
				core_pos, psci_ret);
			return TEST_RESULT_FAIL;
		}
	}

	/* Wait until all CPUs have started the test. */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_has_entered_test[core_pos]);
	}

	/* Enter the test on lead CPU and return the result. */
	return test_spci_non_blocking_fn();
}
