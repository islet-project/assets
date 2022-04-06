/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>

#include <debug.h>
#include <smccc.h>

#include <arch_helpers.h>
#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>
#include <ffa_svc.h>
#include <lib/events.h>
#include <lib/power_management.h>
#include <platform.h>
#include <test_helpers.h>

#include <plat_topology.h>
#include <runtime_services/realm_payload/realm_payload_test.h>

static test_result_t realm_multi_cpu_payload_del_undel(void);

#define ECHO_VAL1 U(0xa0a0a0a0)
#define ECHO_VAL2 U(0xb0b0b0b0)
#define ECHO_VAL3 U(0xc0c0c0c0)

/* Buffer to delegate and undelegate */
static char bufferdelegate[NUM_GRANULES * GRANULE_SIZE * PLATFORM_CORE_COUNT] __aligned(GRANULE_SIZE);
static char bufferstate[NUM_GRANULES * PLATFORM_CORE_COUNT];
static int cpu_test_spm_rmi[PLATFORM_CORE_COUNT];

/*
 * The following test conducts SPM(direct messaging) tests on a subset of selected CPUs while
 * simultaneously performing another set of tests of the RMI(delegation)
 * on the remaining CPU's to the full platform count. Once that test completes
 * the same test is run again with a different assignment for what CPU does
 * SPM versus RMI.
 */

/*
 * Function that randomizes the CPU assignment of tests, SPM or RMI
 */
static void rand_cpu_spm_rmi(void)
{
	int fentry;
	int seln = 0;
	for (int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		cpu_test_spm_rmi[i] = -1;
	}
	for (int i = 0; i < NUM_CPU_DED_SPM; i++) {
		fentry = 0;
		while (fentry == 0) {
#if (PLATFORM_CORE_COUNT > 1)
			seln = (rand() % (PLATFORM_CORE_COUNT - 1)) + 1;
#endif
			if (cpu_test_spm_rmi[seln] == -1) {
				cpu_test_spm_rmi[seln] = 1;
				fentry = 1;
			}
		}
	}
	for (int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (cpu_test_spm_rmi[i] == -1) {
			cpu_test_spm_rmi[i] = 0;
		}
	}
}

/*
 * Get function to determine what has been assigned to a given CPU
 */
static int spm_rmi_test(unsigned int mpidr)
{
	return cpu_test_spm_rmi[platform_get_core_pos(mpidr)];
}

/*
 * RMI function to randomize the initial state of granules allocated for the test.
 * A certain subset will be delegated leaving the rest undelegated
 */
static test_result_t init_buffer_del_spm_rmi(void)
{
	u_register_t retrmm;

	for (int i = 0; i < (NUM_GRANULES * PLATFORM_CORE_COUNT) ; i++) {
		if ((rand() % 2) == 0) {
			retrmm = realm_granule_delegate((u_register_t)&bufferdelegate[i * GRANULE_SIZE]);
			bufferstate[i] = B_DELEGATED;
			if (retrmm != 0UL) {
				tftf_testcase_printf("Delegate operation returns fail, %lx\n", retrmm);
				return TEST_RESULT_FAIL;
			}
		} else {
			bufferstate[i] = B_UNDELEGATED;
		}
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * SPM functions for the direct messaging
 */
static const struct ffa_uuid expected_sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}
	};


static event_t cpu_booted[PLATFORM_CORE_COUNT];

static test_result_t send_cactus_echo_cmd(ffa_id_t sender,
					  ffa_id_t dest,
					  uint64_t value)
{
	smc_ret_values ret;
	ret = cactus_echo_send_cmd(sender, dest, value);

	/*
	 * Return responses may be FFA_MSG_SEND_DIRECT_RESP or FFA_INTERRUPT,
	 * but only expect the former. Expect SMC32 convention from SP.
	 */
	if (!is_ffa_direct_response(ret)) {
		return TEST_RESULT_FAIL;
	}

	if (cactus_get_response(ret) != CACTUS_SUCCESS ||
	    cactus_echo_get_val(ret) != value) {
		ERROR("Echo Failed!\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * Handler that is passed during tftf_cpu_on to individual CPU cores.
 * Runs a specific core and send a direct message request.
 * Expects core_pos | SP_ID as a response.
 */
static test_result_t run_spm_direct_message(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);
	test_result_t ret = TEST_RESULT_SUCCESS;
	smc_ret_values ffa_ret;

	/*
	 * Send a direct message request to SP1 (MP SP) from current physical
	 * CPU. Notice SP1 ECs are already woken as a result of the PSCI_CPU_ON
	 * invocation so they already reached the message loop.
	 * The SPMC uses the MP pinned context corresponding to the physical
	 * CPU emitting the request.
	 */
	ret = send_cactus_echo_cmd(HYP_ID, SP_ID(1), ECHO_VAL1);
	if (ret != TEST_RESULT_SUCCESS) {
		goto out;
	}

	/*
	 * Secure Partitions beyond the first SP only have their first
	 * EC (or vCPU0) woken up at boot time by the SPMC.
	 * Other ECs need one round of ffa_run to reach the message loop.
	 */
	ffa_ret = ffa_run(SP_ID(2), core_pos);
	if (ffa_func_id(ffa_ret) != FFA_MSG_WAIT) {
		ERROR("Failed to run SP%x on core %u\n", SP_ID(2),
				core_pos);
		ret = TEST_RESULT_FAIL;
		goto out;
	}

	/*
	 * Send a direct message request to SP2 (MP SP) from current physical
	 * CPU. The SPMC uses the MP pinned context corresponding to the
	 * physical CPU emitting the request.
	 */
	ret = send_cactus_echo_cmd(HYP_ID, SP_ID(2), ECHO_VAL2);
	if (ret != TEST_RESULT_SUCCESS) {
		goto out;
	}

	/*
	 * Send a direct message request to SP3 (UP SP) from current physical CPU.
	 * The SPMC uses the single vCPU migrated to the new physical core.
	 * The single SP vCPU may receive requests from multiple physical CPUs.
	 * Thus it is possible one message is being processed on one core while
	 * another (or multiple) cores attempt sending a new direct message
	 * request. In such case the cores attempting the new request receive
	 * a busy response from the SPMC. To handle this case a retry loop is
	 * implemented permitting some fairness.
	 */
	uint32_t trial_loop = 5U;
	while (trial_loop--) {
		ffa_ret = cactus_echo_send_cmd(HYP_ID, SP_ID(3), ECHO_VAL3);
		if ((ffa_func_id(ffa_ret) == FFA_ERROR) &&
		    (ffa_error_code(ffa_ret) == FFA_ERROR_BUSY)) {
			VERBOSE("%s(%u) trial %u\n", __func__, core_pos, trial_loop);
			waitms(1);
			continue;
		}

		if (is_ffa_direct_response(ffa_ret) == true) {
			if (cactus_get_response(ffa_ret) != CACTUS_SUCCESS ||
				cactus_echo_get_val(ffa_ret) != ECHO_VAL3) {
				ERROR("Echo Failed!\n");
				ret = TEST_RESULT_FAIL;
			}

			goto out;
		}
	}

	ret = TEST_RESULT_FAIL;

out:
	/* Tell the lead CPU that the calling CPU has completed the test */
	tftf_send_event(&cpu_booted[core_pos]);

	return ret;
}

/*
 * Test function to dispatch a number of SPM and RMI tests to the platform
 * number of CPU's.  The test is run twice on a another set of randomly determined
 * CPU's.
 */
test_result_t test_ffa_secondary_core_direct_realm_msg(void)
{
	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_node, mpidr;
	int32_t ret;
	u_register_t retrmm;

	/**********************************************************************
	 * Check SPMC has ffa_version and expected FFA endpoints are deployed.
	 **********************************************************************/
	CHECK_SPMC_TESTING_SETUP(1, 0, expected_sp_uuids);

	for (unsigned int i = 0U; i < PLATFORM_CORE_COUNT; i++) {
		tftf_init_event(&cpu_booted[i]);
	}

	/*
	 * Randomize the assignment of the CPU's to either SPM or RMI
	 */
	rand_cpu_spm_rmi();

	/*
	 * Randomize the initial state of the RMI granules to realm or non-secure
	 */
	if (init_buffer_del_spm_rmi() == TEST_RESULT_FAIL) {
		return TEST_RESULT_FAIL;
	}

	/*
	 * Main test to run both SPM and RMI together
	 */
	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == lead_mpid) {
			continue;
		}

		if (spm_rmi_test(mpidr) == 1) {
			ret = tftf_cpu_on(mpidr, (uintptr_t)run_spm_direct_message, 0U);
			if (ret != 0) {
				ERROR("tftf_cpu_on mpidr 0x%x returns %d\n", mpidr, ret);
				return TEST_RESULT_FAIL;
			}
		} else {
			ret = tftf_cpu_on(mpidr,
				(uintptr_t)realm_multi_cpu_payload_del_undel, 0);

			if (ret != PSCI_E_SUCCESS) {
				ERROR("CPU ON failed for 0x%llx\n",
					(unsigned long long)mpidr);
				return TEST_RESULT_FAIL;
			}
		}
	}

	VERBOSE("Waiting for secondary CPUs to turn off ...\n");

	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == lead_mpid) {
			continue;
		}

		while (tftf_psci_affinity_info(mpidr, MPIDR_AFFLVL0) !=
				PSCI_STATE_OFF) {
				continue;
		}
	}

	/*
	 * Randomize the CPU's again
	 */
	rand_cpu_spm_rmi();

	/*
	 * Run test again
	 */
	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == lead_mpid) {
			continue;
		}

		if (spm_rmi_test(mpidr) == 1) {
			ret = tftf_cpu_on(mpidr, (uintptr_t)run_spm_direct_message, 0U);
			if (ret != 0) {
				ERROR("tftf_cpu_on mpidr 0x%x returns %d\n", mpidr, ret);
				return TEST_RESULT_FAIL;
			}
		} else {
			ret = tftf_cpu_on(mpidr,
				(uintptr_t)realm_multi_cpu_payload_del_undel, 0);

			if (ret != PSCI_E_SUCCESS) {
				ERROR("CPU ON failed for 0x%llx\n",
					(unsigned long long)mpidr);
				return TEST_RESULT_FAIL;
			}
		}
	}

	VERBOSE("Waiting for secondary CPUs to turn off ...\n");

	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == lead_mpid) {
			continue;
		}

		while (tftf_psci_affinity_info(mpidr, MPIDR_AFFLVL0) !=
				PSCI_STATE_OFF) {
			continue;
		}

	}

	for (int i = 0; i < (NUM_GRANULES * PLATFORM_CORE_COUNT) ; i++) {
		if (bufferstate[i] == B_DELEGATED) {
			retrmm = realm_granule_undelegate(
				(u_register_t)&bufferdelegate[i * GRANULE_SIZE]);
			bufferstate[i] = B_UNDELEGATED;
			if (retrmm != 0UL) {
				tftf_testcase_printf("Delegate operation returns fail, %lx\n", retrmm);
				return TEST_RESULT_FAIL;
			}
		}
	}

	VERBOSE("Done exiting.\n");

	/**********************************************************************
	 * All tests passed.
	 **********************************************************************/

	return TEST_RESULT_SUCCESS;
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

	for (int i = 0; i < NUM_GRANULES; i++) {
		if (bufferstate[((cpu_node * NUM_GRANULES) + i)] == B_UNDELEGATED) {
			retrmm = realm_granule_delegate((u_register_t)
					&bufferdelegate[((cpu_node *
						NUM_GRANULES) + i) * GRANULE_SIZE]);
			bufferstate[((cpu_node * NUM_GRANULES) + i)] = B_DELEGATED;
		} else {
			retrmm = realm_granule_undelegate((u_register_t)
					&bufferdelegate[((cpu_node *
						NUM_GRANULES) + i) * GRANULE_SIZE]);
			bufferstate[((cpu_node * NUM_GRANULES) + i)] = B_UNDELEGATED;
		}
		if (retrmm != 0UL) {
			tftf_testcase_printf("Delegate operation returns fail, %lx\n", retrmm);
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}
