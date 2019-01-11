/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <drivers/arm/gic_v2.h>
#include <events.h>
#include <irq.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <stdlib.h>
#include <test_helpers.h>
#include <tftf.h>
#include <tftf_lib.h>
#include <timer.h>

#define MAX_TEST_ITERATIONS (100 * PLATFORM_CORE_COUNT)

/* Number of iterations of the test */
static int iteration_count;

/* The CPU assigned the baton to drive the test */
static u_register_t baton_cpu;

/* Synchronization event which will be waited on by all the non-baton CPUs */
static event_t sync_event;

/* Global variables to synchronize participating CPUs on wake-up */
static spinlock_t cpu_count_lock;
static volatile int cpu_count;
static volatile int participating_cpu_count;

/* Variable to store the system suspend power state and its statistics */
static int system_susp_pwr_state;
static u_register_t susp_count;

static test_result_t do_sys_susp_on_off_stress(void);

/*
 * Helper function to wait for participating CPUs participating to enter the
 * test function.
 */
static void wait_for_cpus_to_enter_test(void)
{
	assert(participating_cpu_count <= PLATFORM_CORE_COUNT);
	while (cpu_count != participating_cpu_count)
		;
}

/* Helper function to increment the cpu_count */
static void inc_cpu_count(void)
{
	spin_lock(&cpu_count_lock);
	cpu_count++;
	spin_unlock(&cpu_count_lock);
	assert(cpu_count <= PLATFORM_CORE_COUNT);
}

/* Helper function to decrement the cpu_count */
static void dec_cpu_count(void)
{
	spin_lock(&cpu_count_lock);
	cpu_count--;
	spin_unlock(&cpu_count_lock);
	assert(cpu_count >= 0);
}

/* Helper function to turn ON all the CPUs in the platform */
static int try_cpu_on_all(void)
{
	int ret, cpu_node;
	u_register_t cpu_mpid, current_cpu = read_mpidr_el1() & MPID_MASK;

	/* Try to turn on all the non-lead CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);

		/* Skip lead CPU, it is already powered on */
		if (cpu_mpid == current_cpu)
			continue;

		do {
			ret = tftf_try_cpu_on(cpu_mpid,
				(uintptr_t) do_sys_susp_on_off_stress, 0);
			if (ret != PSCI_E_SUCCESS && ret != PSCI_E_ON_PENDING &&
					ret != PSCI_E_ALREADY_ON) {
				ERROR("Unexpected return value 0x%x"
						" from PSCI CPU ON\n", ret);
				return -1;
			}
		} while (ret != PSCI_E_SUCCESS);
	}
	return 0;
}

/* Helper function function to get number of CPUs which are OFF in the system */
static int get_off_cpu_count(void)
{
	int aff_off_cpus = 0;
	u_register_t cpu_mpid, current_cpu = read_mpidr_el1() & MPID_MASK;
	int cpu_node;

	/* Query the number of OFF CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU, it is already powered on */
		if (cpu_mpid == current_cpu)
			continue;

		if (tftf_psci_affinity_info(cpu_mpid, MPIDR_AFFLVL0) ==
					PSCI_STATE_OFF)
			aff_off_cpus++;
	}

	return aff_off_cpus;
}

/*
 * The main test function which will be executed by all CPUs.
 * 1. The CPU holding the baton will first enter this function and then turns
 *    ON all other CPUs.
 * 2. All the `non-baton` CPUs then wait for the `sync_event` to be signaled
 *    to turn themselves OFF.
 * 3. Number of CPUs which are signaled via `sync_event` by baton CPU is
 *    random.
 * 4. After signaled CPUs have turned themselves OFF, SYSTEM SUSPEND is
 *    issued by the baton CPU.
 * 5. The return value of SYSTEM_SUSPEND is checked by the baton CPU.
 * 6. The next baton CPU is chosen randomly and the test is handed over to this
 *    CPU.
 */
static test_result_t do_sys_susp_on_off_stress(void)
{
	int psci_ret, off_cpu_count;
	u_register_t current_cpu;

	inc_cpu_count();

	current_cpu = read_mpidr_el1() & MPID_MASK;
	if (current_cpu != baton_cpu) {
		tftf_wait_for_event(&sync_event);
		dec_cpu_count();
		return TEST_RESULT_SUCCESS;
	}

	INFO("System suspend test: Baton holder CPU = 0x%llx\n",
			(unsigned long long) current_cpu);
	if (try_cpu_on_all() == -1) {
		tftf_testcase_printf("CPU_ON of secondary CPUs failed.\n");
		return TEST_RESULT_FAIL;
	}

	wait_for_cpus_to_enter_test();

	/* Turn off random number of cores 1 out of 3 times */
	if (rand() % 3)
		off_cpu_count = rand() % participating_cpu_count;
	else
		off_cpu_count = participating_cpu_count - 1;

	/* Signal random number of CPUs to turn OFF */
	tftf_send_event_to(&sync_event, off_cpu_count);

	/* Wait for `off_cpu_count` CPUs to turn OFF */
	while (get_off_cpu_count() != off_cpu_count)
		;

	/* Program timer to fire after delay */
	tftf_program_timer(PLAT_SUSPEND_ENTRY_TIME);

	/* Issue SYSTEM SUSPEND */
	psci_ret = tftf_system_suspend();
	tftf_cancel_timer();

	/* Check return value of SYSTEM SUSPEND API */
	if (off_cpu_count == (participating_cpu_count - 1)) {
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("SYSTEM SUSPEND did not succeed "
							"where expected\n");
			return TEST_RESULT_FAIL;
		}
	} else {
		if (psci_ret != PSCI_E_DENIED) {
			tftf_testcase_printf("SYSTEM SUSPEND did not fail "
							"where expected\n");
			return TEST_RESULT_FAIL;
		}
	}

	/* Pass the baton top another CPU */
	baton_cpu = tftf_find_random_cpu_other_than(current_cpu);

	/* Unblock the waiting CPUs */
	tftf_send_event_to(&sync_event,
			(participating_cpu_count - 1) - off_cpu_count);

	/* Wait for all CPUs other than current to turn OFF */
	while (get_off_cpu_count() != (participating_cpu_count - 1))
		;

	dec_cpu_count();

	if (iteration_count++ < MAX_TEST_ITERATIONS) {
		/* Hand over the test execution the new baton CPU */
		psci_ret = tftf_cpu_on(baton_cpu,
				(uintptr_t) do_sys_susp_on_off_stress, 0);
		if (psci_ret != PSCI_E_SUCCESS)
			return TEST_RESULT_FAIL;

		/* Wait for new baton CPU to enter test */
		while (cpu_count == 0)
			;
	} else {
		/*
		 * The test has completed. Print statistics if PSCI STAT COUNT
		 * is supported.
		 */
		if (is_psci_stat_count_supported()) {
			u_register_t count = tftf_psci_stat_count(baton_cpu,
					system_susp_pwr_state);
			tftf_testcase_printf("Iterated %d with %lld system"
				" suspends\n", MAX_TEST_ITERATIONS,
				(unsigned long long)(count - susp_count));
		}
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Stress test PSCI SYSTEM SUSPEND API.
 * This test iteratively issues PSCI SYSTEM SUSPEND on random cores after
 * issuing turning OFF a random number of CPUs. The PSCI SYSTEM SUSPEND
 * will only succeed if all the CPUs except the calling CPU is OFF.
 */
test_result_t psci_sys_susp_on_off_stress_test(void)
{
	unsigned int pstateid_idx[PLAT_MAX_PWR_LEVEL + 1];
	unsigned int pwrlvl, susp_type, state_id;
	int ret;

	if (!is_psci_sys_susp_supported()) {
		tftf_testcase_printf("System suspend is not supported "
				"by the EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	INIT_PWR_LEVEL_INDEX(pstateid_idx);
	tftf_init_event(&sync_event);
	init_spinlock(&cpu_count_lock);

	/* Initialize participating CPU count */
	participating_cpu_count = tftf_get_total_cpus_count();
	cpu_count = 0;

	iteration_count = 0;

	/*
	 * Assign a baton to the current CPU and it is in charge of driving
	 * the test.
	 */
	baton_cpu = read_mpidr_el1() & MPID_MASK;

	/* Print SYSTEM SUSPEND statistics if PSCI STAT is supported */
	if (is_psci_stat_count_supported()) {
		NOTICE("PSCI STAT COUNT supported\n");
		tftf_set_deepest_pstate_idx(PLAT_MAX_PWR_LEVEL, pstateid_idx);

		/* Check if the power state is valid */
		ret = tftf_get_pstate_vars(&pwrlvl,
					&susp_type,
					&state_id,
					pstateid_idx);
		if (ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("tftf_get_pstate_vars() failed"
					" with ret = %x\n", ret);
			return TEST_RESULT_FAIL;
		}

		assert(pwrlvl == PLAT_MAX_PWR_LEVEL);

		system_susp_pwr_state = tftf_make_psci_pstate(pwrlvl,
				susp_type, state_id);

		susp_count = tftf_psci_stat_count(baton_cpu, system_susp_pwr_state);
	}

	return do_sys_susp_on_off_stress();
}
