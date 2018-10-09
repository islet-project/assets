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
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <test_helpers.h>
#include <tftf_lib.h>

/* Invoke _func and verifies return value is TEST_RESULT_SUCCESS */
#define TEST_FUNC(_func) {							\
	int ret = (_func)();							\
	if (ret != TEST_RESULT_SUCCESS) {					\
		INFO("test_node_hw_state: function " #_func " failed!\n");	\
		return ret;							\
	}									\
}

/* Enable messages for debugging purposes */
#if 0
# define DBGMSG(...)	INFO(__VA_ARGS__)
#else
# define DBGMSG(...)
#endif

#define INVALID_POWER_LEVEL	(PLAT_MAX_PWR_LEVEL + 1)

static event_t cpu_booted[PLATFORM_CORE_COUNT];
static event_t cpu_continue[PLATFORM_CORE_COUNT];

/* MPIDRs for CPU belonging to both own and foreign clusters */
static u_register_t native_peer = INVALID_MPID;
static u_register_t foreign_peer = INVALID_MPID;

static test_result_t cpu_ping(void)
{
	u_register_t mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	/* Wait for flag to proceed */
	tftf_wait_for_event(&cpu_continue[core_pos]);

	/*
	 * When returning from the function, the TFTF framework will power CPUs
	 * down, without this test needing to do anything
	 */
	return TEST_RESULT_SUCCESS;
}

/* Helper function to detect support for PSCI NODE_HW_STATE */
static int is_psci_node_hw_state_supported(void)
{
	return (tftf_get_psci_feature_info(SMC_PSCI_CPU_HW_STATE64) ==
			PSCI_E_NOT_SUPPORTED) ? 0 : 1;
}

/*
 * @Test_Aim@ Call NODE_HW_STATE for the current CPU and make sure it returns
 * PSCI_HW_STATE_ON
 */
static test_result_t test_self_cpu(void)
{
	if (tftf_psci_node_hw_state(read_mpidr_el1(), 0) != PSCI_HW_STATE_ON) {
		DBGMSG("%s: failed\n", __func__);
		return TEST_RESULT_FAIL;
	} else {
		return TEST_RESULT_SUCCESS;
	}
}

/*
 * @Test_Aim@ Call NODE_HW_STATE for the current cluster and make sure it
 * returns PSCI_HW_STATE_ON
 */
static test_result_t test_self_cluster(void)
{
	if (tftf_psci_node_hw_state(read_mpidr_el1(), 1) != PSCI_HW_STATE_ON) {
		DBGMSG("%s: failed\n", __func__);
		return TEST_RESULT_FAIL;
	} else {
		return TEST_RESULT_SUCCESS;
	}
}

/*
 * @Test_Aim@ Call NODE_HW_STATE for a foreign CPU that's currently off. Make
 * sure it returns PSCI_HW_STATE_OFF
 */
static test_result_t test_offline_cpu(void)
{
	assert(foreign_peer != INVALID_MPID);
	if (tftf_psci_node_hw_state(foreign_peer, 0) != PSCI_HW_STATE_OFF) {
		DBGMSG("%s: failed\n", __func__);
		return TEST_RESULT_FAIL;
	} else {
		return TEST_RESULT_SUCCESS;
	}
}

/*
 * @Test_Aim@ Call NODE_HW_STATE for a cluster that's currently off. Make sure
 * it returns PSCI_HW_STATE_OFF
 */
static test_result_t test_offline_cluster(void)
{
	assert(foreign_peer != INVALID_MPID);
	if (tftf_psci_node_hw_state(foreign_peer, 1) != PSCI_HW_STATE_OFF) {
		DBGMSG("%s: failed\n", __func__);
		return TEST_RESULT_FAIL;
	} else {
		return TEST_RESULT_SUCCESS;
	}
}

/*
 * @Test_Aim@ Call NODE_HW_STATE with an invalid MPIDR. Make sure it returns
 * invalid parameters
 */
static test_result_t test_invalid_mpidr(void)
{
	if (tftf_psci_node_hw_state(INVALID_MPID, 0) != PSCI_E_INVALID_PARAMS) {
		DBGMSG("%s: failed\n", __func__);
		return TEST_RESULT_FAIL;
	} else {
		return TEST_RESULT_SUCCESS;
	}
}

/*
 * @Test_Aim@ Call NODE_HW_STATE with an invalid power_level. Make sure it
 * returns invalid parameters
 */
static test_result_t test_invalid_power_level(void)
{
	if (tftf_psci_node_hw_state(read_mpidr_el1(), INVALID_POWER_LEVEL) !=
			PSCI_E_INVALID_PARAMS) {
		DBGMSG("%s: failed\n", __func__);
		return TEST_RESULT_FAIL;
	} else {
		return TEST_RESULT_SUCCESS;
	}
}

/*
 * @Test_Aim@ Call NODE_HW_STATE on all powered-down CPUs on the system. Verify
 * that the state was PSCI_HW_STATE_OFF before, but is PSCI_HW_STATE_ON
 * afterwards
 */
static test_result_t test_online_all(void)
{
	int cpu_node, pos, state, ret, i;
	u_register_t mpidr, my_mpidr;

	/* Initialize all events */
	for (i = 0; i < ARRAY_SIZE(cpu_booted); i++)
		tftf_init_event(&cpu_booted[i]);
	for (i = 0; i < ARRAY_SIZE(cpu_continue); i++)
		tftf_init_event(&cpu_continue[i]);

	DBGMSG("%s: powering cores on...\n", __func__);
	my_mpidr = read_mpidr_el1() & MPID_MASK;
	DBGMSG("%s: my mpidr: %llx\n", __func__,
			(unsigned long long) my_mpidr);
	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == my_mpidr)
			continue;

		/* Verify that the other CPU is turned off */
		state = tftf_psci_node_hw_state(mpidr, 0);
		if (state != PSCI_HW_STATE_OFF) {
			DBGMSG("%s: before: mpidr %llx: state %u, expected %u\n",
					__func__, (unsigned long long) mpidr,
					state, PSCI_HW_STATE_OFF);
			return TEST_RESULT_FAIL;
		}

		/* Power on the CPU and wait for its event */
		pos = platform_get_core_pos(mpidr);
		ret = tftf_cpu_on(mpidr, (uintptr_t) cpu_ping, 0);
		if (ret != PSCI_E_SUCCESS) {
			DBGMSG("%s: powering on %llx failed", __func__,
					(unsigned long long)mpidr);
			return TEST_RESULT_FAIL;
		}
		tftf_wait_for_event(&cpu_booted[pos]);

		/* Verify that the other CPU is turned on */
		state = tftf_psci_node_hw_state(mpidr, 0);
		if (state != PSCI_HW_STATE_ON) {
			DBGMSG("%s: after: mpidr %llx: state %u, expected %u\n",
					__func__, (unsigned long long)mpidr,
					state, PSCI_HW_STATE_ON);
			return TEST_RESULT_FAIL;
		}

		/* Allow to the CPU to proceed to power down */
		tftf_send_event(&cpu_continue[pos]);
	}

	/* Wait for other CPUs to power down */
	INFO("%s: waiting for all other CPUs to power down\n", __func__);
	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == my_mpidr)
			continue;

		/* Loop until other CPU is powered down */
		while (tftf_psci_affinity_info(mpidr, MPIDR_AFFLVL0) !=
				PSCI_STATE_OFF)
			tftf_timer_sleep(10);
	}

	/* Now verify that all CPUs have powered off */
	for_each_cpu(cpu_node) {
		mpidr = tftf_get_mpidr_from_node(cpu_node);
		if (mpidr == my_mpidr)
			continue;

		/* Verify that the other CPU is turned off */
		state = tftf_psci_node_hw_state(mpidr, 0);
		if (state != PSCI_HW_STATE_OFF) {
			DBGMSG("%s: mpidr %llx: state %u, expected %u\n",
					__func__, (unsigned long long)mpidr,
					state, PSCI_HW_STATE_OFF);
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Find a peer CPU in the system. The 'foreign' argument specifies where to
 * locate the peer CPU: value zero finds a CPU in the same cluster; non-zero
 * argument finds CPU from a different cluster.
 */
static u_register_t find_peer(int foreign)
{
	int dmn, cpu;
	u_register_t mpidr, my_mpidr;

	my_mpidr = read_mpidr_el1() & MPID_MASK;
	dmn = PWR_DOMAIN_INIT;
	cpu = PWR_DOMAIN_INIT;
	do {
		dmn = tftf_get_next_peer_domain(dmn, foreign);
		if (foreign) {
			cpu = tftf_get_next_cpu_in_pwr_domain(dmn,
					PWR_DOMAIN_INIT);
		} else {
			cpu = dmn;
		}

		assert(cpu != PWR_DOMAIN_INIT);
		mpidr = tftf_get_mpidr_from_node(cpu);
		assert(mpidr != INVALID_MPID);
	} while (mpidr == my_mpidr && dmn != PWR_DOMAIN_INIT);

	return mpidr;
}

/*
 * @Test_Aim@ Validate PSCI NODE_HW_STATE API
 */
test_result_t test_psci_node_hw_state(void)
{
	DBGMSG("%s: begin\n", __func__);
	if (!is_psci_node_hw_state_supported()) {
		tftf_testcase_printf("PSCI NODE_HW_STATE is not supported\n");
		return TEST_RESULT_SKIPPED;
	}

	TEST_FUNC(test_invalid_mpidr);
	TEST_FUNC(test_invalid_power_level);
	TEST_FUNC(test_self_cpu);
	TEST_FUNC(test_self_cluster);
	TEST_FUNC(test_online_all);

	DBGMSG("%s: end\n", __func__);
	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Validate PSCI NODE_HW_STATE API in sytems with more than one
 * cluster
 */
test_result_t test_psci_node_hw_state_multi(void)
{
	SKIP_TEST_IF_LESS_THAN_N_CLUSTERS(2);

	DBGMSG("%s: begin\n", __func__);
	if (!is_psci_node_hw_state_supported()) {
		tftf_testcase_printf("PSCI NODE_HW_STATE is not supported\n");
		return TEST_RESULT_SKIPPED;
	}

	/* Initialize peer MPDIRs */
	native_peer = find_peer(0);
	foreign_peer = find_peer(1);
	DBGMSG("native=%x foreign=%x\n", native_peer, foreign_peer);

	TEST_FUNC(test_offline_cpu);
	TEST_FUNC(test_offline_cluster);

	DBGMSG("%s: end\n", __func__);
	return TEST_RESULT_SUCCESS;
}
