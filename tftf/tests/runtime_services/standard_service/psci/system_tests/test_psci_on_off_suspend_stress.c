/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <debug.h>
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

#define STRESS_TEST_COUNT 100

/* Per-CPU counters used for the coherency test */
typedef struct cpu_pm_ops_desc {
	spinlock_t lock;
	unsigned int pcpu_count[PLATFORM_CORE_COUNT];
} cpu_pm_ops_desc_t;

static cpu_pm_ops_desc_t device_pm_ops_desc
			__attribute__ ((section("tftf_coherent_mem")));
static cpu_pm_ops_desc_t normal_pm_ops_desc;

static event_t cpu_booted[PLATFORM_CORE_COUNT];
static volatile unsigned int start_test;
static unsigned int exit_test;
static unsigned int power_state;
/* The target for CPU ON requests */
static volatile unsigned long long target_mpid;

static spinlock_t counter_lock;
static volatile unsigned int cpu_on_count;
/* Whether CPU suspend calls should be thrown into the test */
static unsigned int include_cpu_suspend;

static test_result_t secondary_cpu_on_race_test(void);

/*
 * Utility function to wait for all CPUs other than the caller to be
 * OFF.
 */
static void wait_for_non_lead_cpus(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int target_mpid, target_node;

	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU, as it is powered on */
		if (target_mpid == lead_mpid)
			continue;

		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0)
				!= PSCI_STATE_OFF)
			;
	}
}

/*
 * Update per-cpu counter corresponding to the current CPU.
 * This function updates 2 counters, one in normal memory and the other
 * in coherent device memory. The counts are then compared to check if they
 * match. This verifies that the caches and the interconnect are coherent
 * during the test.
 * Returns -1 on error, 0 on success.
 */
static int update_counters(void)
{
	unsigned int normal_count, device_count;
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());

	/*
	 * Ensure that the copies of the counters in device and normal memory
	 * match. The locks and the data should become incoherent if any cluster
	 * is not taking part in coherency.
	 */
	spin_lock(&normal_pm_ops_desc.lock);
	normal_count = normal_pm_ops_desc.pcpu_count[core_pos];
	spin_unlock(&normal_pm_ops_desc.lock);

	spin_lock(&device_pm_ops_desc.lock);
	device_count = device_pm_ops_desc.pcpu_count[core_pos];
	spin_unlock(&device_pm_ops_desc.lock);

	if (device_count != normal_count) {
		tftf_testcase_printf("Count mismatch. Device memory count ="
				" %u: normal memory count = %u\n",
				device_count, normal_count);
		return -1;
	}

	/* Increment the count in both copies of the counter */
	spin_lock(&normal_pm_ops_desc.lock);
	normal_pm_ops_desc.pcpu_count[core_pos]++;
	spin_unlock(&normal_pm_ops_desc.lock);

	spin_lock(&device_pm_ops_desc.lock);
	device_pm_ops_desc.pcpu_count[core_pos]++;
	spin_unlock(&device_pm_ops_desc.lock);

	return 0;
}

/*
 * The test loop for non lead CPUs in psci_on_off_suspend_coherency_test. It
 * updates the counters and depending on the value of the random variable `op`,
 * the secondaries either offlines (by returning back to Test framework) or
 * suspends itself.
 */
static test_result_t random_suspend_off_loop(void)
{
#define OFFLINE_CORE	1
#define SUSPEND_CORE	0

	int rc, op;

	while (!exit_test) {
		rc = update_counters();
		if (rc)
			return TEST_RESULT_FAIL;

		/* Find what we will be doing next */
		op = rand() % 2;

		/*
		 * If the chosen action is to power off, then return from the
		 * test function so that the test framework powers this CPU off.
		 */
		if (op == OFFLINE_CORE)
			return TEST_RESULT_SUCCESS;

		/* Program timer for wake-up event. */
		rc = tftf_program_timer_and_suspend(PLAT_SUSPEND_ENTRY_TIME,
				power_state, NULL, NULL);

		tftf_cancel_timer();

		if (rc != PSCI_E_SUCCESS) {
			tftf_testcase_printf("CPU timer/suspend returned error"
							" 0x%x\n", rc);
			return TEST_RESULT_FAIL;
		}
	}
	return TEST_RESULT_SUCCESS;
}

static test_result_t lead_cpu_main(unsigned long long mpid)
{
	unsigned int rc;
	unsigned long long rand_mpid;
	int i;

	/* The lead cpu will not be turned off. */
	for (i = STRESS_TEST_COUNT; i >= 0; i--) {
		rc = update_counters();
		if (rc)
			return TEST_RESULT_FAIL;

		/* Program timer for wake-up event. */
		rc = tftf_program_timer_and_suspend(PLAT_SUSPEND_ENTRY_TIME,
				power_state, NULL, NULL);

		tftf_cancel_timer();

		if (rc != PSCI_E_SUCCESS) {
			tftf_testcase_printf("CPU timer/suspend returned error"
							" 0x%x\n", rc);
			return TEST_RESULT_FAIL;
		}

		/*
		 * The lead cpu's woken up since the system timer has fired.
		 * For any cpus which have turned themselves off, generate a
		 * random MPIDR and try turning on the corresponding cpu.
		 */
		do {
			rand_mpid = tftf_find_random_cpu_other_than(mpid);
		} while (tftf_psci_affinity_info(rand_mpid, MPIDR_AFFLVL0)
				  != PSCI_STATE_OFF);

		rc = tftf_try_cpu_on(rand_mpid,
				      (uintptr_t) random_suspend_off_loop,
				      0);
		if ((rc != PSCI_E_ALREADY_ON) &&
		    (rc != PSCI_E_ON_PENDING) &&
		    (rc != PSCI_E_SUCCESS) &&
		    (rc != PSCI_E_INVALID_PARAMS)) {
			tftf_testcase_printf("CPU ON failed with error ="
								" 0x%x\n", rc);
			return TEST_RESULT_FAIL;
		}
	}

	exit_test = 1;
	/* Ensure update to `exit_test` is seen by all cores prior to
	   invoking wait_for_non_lead_cpus() */
	dmbsy();

	wait_for_non_lead_cpus();

	INFO("Exiting test\n");
	return TEST_RESULT_SUCCESS;
}

/*
 * Send event depicting CPU booted successfully and then invoke
 * random_suspend_off_loop.
 */
static test_result_t non_lead_random_suspend_off_loop(void)
{
	unsigned long long mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	return random_suspend_off_loop();
}

/*
 * @Test_Aim@ Repeated cores hotplug as stress test
 * Test Do :
 * 1) Power up all the cores
 * 2) Ensure all the cores have booted successfully to TFTF
 * 3) Randomly suspend or turn OFF secondary CPU
 * 4) The lead CPU will suspend and turn ON a random CPU which has powered OFF.
 * 5) Repeat 1-4 STRESS_TEST_COUNT times
 * 6) The test is aborted straight away if any failure occurs. In this case,
 *    the test is declared as failed.
 * Note: The test will be skipped on single-core platforms.
 */
test_result_t psci_on_off_suspend_coherency_test(void)
{
	unsigned int cpu_node, core_pos;
	unsigned long long cpu_mpid, lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int counter_lo, stateid;
	int psci_ret;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	/* Reinitialize the event variable */
	for (unsigned int j = 0; j < PLATFORM_CORE_COUNT; ++j)
		tftf_init_event(&cpu_booted[j]);

	init_spinlock(&normal_pm_ops_desc.lock);
	init_spinlock(&device_pm_ops_desc.lock);

	exit_test = 0;

	/* Seed the random number generator */
	counter_lo = (unsigned int) read_cntpct_el0();
	srand(counter_lo);

	psci_ret = tftf_psci_make_composite_state_id(PLAT_MAX_PWR_LEVEL,
					PSTATE_TYPE_POWERDOWN, &stateid);
	if (psci_ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to construct composite state\n");
		return TEST_RESULT_SKIPPED;
	}
	power_state = tftf_make_psci_pstate(MPIDR_AFFLVL0,
					PSTATE_TYPE_POWERDOWN, stateid);

	/* Turn on all the non-lead CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU, it is already powered on */
		if (cpu_mpid == lead_mpid)
			continue;

		psci_ret = tftf_cpu_on(cpu_mpid,
				(uintptr_t) non_lead_random_suspend_off_loop, 0);
		if (psci_ret != PSCI_E_SUCCESS)
			return TEST_RESULT_FAIL;
	}

	/*
	 * Confirm the non-lead cpus booted and participated in the test
	 */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU, it is already powered on */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_booted[core_pos]);
	}

	core_pos = platform_get_core_pos(lead_mpid);
	return lead_cpu_main(lead_mpid);
}

/*
 * Frantically send CPU ON requests to the target mpidr till it returns
 * ALREADY_ON.
 * Return 0 on success, 1 on error.
 */
static int test_cpu_on_race(void)
{
	int ret;

	do {
		ret = tftf_try_cpu_on(target_mpid,
				(uintptr_t) secondary_cpu_on_race_test, 0);
		if (ret != PSCI_E_SUCCESS && ret != PSCI_E_ON_PENDING &&
				ret != PSCI_E_ALREADY_ON) {
			tftf_testcase_printf("Unexpected return value 0x%x"
					" from PSCI CPU ON\n", ret);
			return 1;
		}
	} while (ret != PSCI_E_ALREADY_ON);

	return 0;
}

/*
 * This function runs the test_cpu_on_race() till either `exit_test`
 * is set or the `target_mpid` is the current mpid.
 */
static test_result_t secondary_cpu_on_race_test(void)
{
	unsigned long long mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);
	int ret;

	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_booted[core_pos]);

	/* Wait for start flag */
	while (start_test == 0)
		;

	do {
		/*
		 * If the current CPU is the target mpid, then power OFF.
		 * The target mpid will be target for CPU ON requests by other
		 * cores.
		 */
		if (mpid == target_mpid)
			return TEST_RESULT_SUCCESS;

		ret = test_cpu_on_race();
		if (ret)
			return TEST_RESULT_FAIL;
	} while (exit_test == 0);

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Verify that the CPU ON race conditions are handled in Firmware.
 * Test Do :
 * 1. Designate a target CPU to power down.
 * 2. Boot up all the CPUs on the system
 * 3. If the CPU is the designated target CPU, power OFF
 * 4. All the other cores issue CPU ON to the target CPU continuously.
 *    As per PSCI specification, only one CPU ON call will succeed in sending
 *    the ON command to the target CPU.
 * 5. The Target CPU should turn ON and execute successfully.
 * 6. The test should iterate again with another CPU as the target CPU this
 *    time.
 * 7. Repeat Steps 1-6 for STRESS_TEST_COUNT times.
 * Note: The test will be skipped on single-core platforms.
 */
test_result_t psci_verify_cpu_on_race(void)
{
	unsigned int cpu_node, core_pos, target_node, j;
	unsigned long long lead_mpid = read_mpidr_el1() & MPID_MASK, cpu_mpid;
	int ret, rc = 0;

	exit_test = 0;
	start_test = 0;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	/* Reinitialize the event variable */
	for (j = 0; j < PLATFORM_CORE_COUNT; j++)
		tftf_init_event(&cpu_booted[j]);

	/* Turn ON all other CPUs */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);

		/* Skip the lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		ret = tftf_cpu_on(cpu_mpid,
				(uintptr_t) secondary_cpu_on_race_test, 0);
		if (ret != PSCI_E_SUCCESS)
			return TEST_RESULT_FAIL;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_booted[core_pos]);
	}

	for (j = 0; j < STRESS_TEST_COUNT; j++) {
		/* Choose a target CPU */
		for_each_cpu(target_node) {
			cpu_mpid = tftf_get_mpidr_from_node(target_node);

			/* Skip the lead CPU */
			if (cpu_mpid == lead_mpid)
				continue;

			target_mpid = cpu_mpid;
			/*
			 * Ensure target_mpid update is visible prior to
			 * starting test.
			 */
			dmbsy();

			VERBOSE("Target MPID = %llx\n", target_mpid);
			start_test = 1;

			/* Wait for the target CPU to turn OFF */
			while (tftf_psci_affinity_info(target_mpid,
					MPIDR_AFFLVL0) != PSCI_STATE_OFF)
				;
			rc = test_cpu_on_race();
			if (rc)
				break;
		}
		if (rc)
			break;
	}
	exit_test = 1;
	wait_for_non_lead_cpus();
	return rc ? TEST_RESULT_FAIL : TEST_RESULT_SUCCESS;
}

/*
 * The Test function to stress test CPU ON/OFF PSCI APIs executed by all CPUs.
 * This function maintains a global counter which is incremented atomically
 * when a CPU enters this function via warm boot (in case of secondary) or
 * direct invocation (in case of lead CPU). It sends CPU ON to all OFF CPUs
 * till another CPU has entered this function or `exit_flag` is set.
 * If `include_cpu_suspend` is set, then it suspends the current CPU
 * before iterating in the loop to send CPU ON requests.
 */
static test_result_t launch_cpu_on_off_stress(void)
{
	unsigned long long mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_node, temp_count;
	int ret;

	spin_lock(&counter_lock);
	/* Store the count in a temporary variable */
	temp_count = ++cpu_on_count;
	spin_unlock(&counter_lock);

	if (exit_test)
		return TEST_RESULT_SUCCESS;

	while (!exit_test) {
		for_each_cpu(cpu_node) {
			mpid = tftf_get_mpidr_from_node(cpu_node);

			if (tftf_is_cpu_online(mpid))
				continue;

			ret = tftf_try_cpu_on(mpid,
					(uintptr_t) launch_cpu_on_off_stress, 0);
			if (ret != PSCI_E_SUCCESS && ret !=
				PSCI_E_ON_PENDING && ret != PSCI_E_ALREADY_ON) {
				tftf_testcase_printf("Unexpected return value"
					" 0x%x from PSCI CPU ON\n", ret);
				return TEST_RESULT_FAIL;
			}
		}

		/* Break if another CPU has entered this test function */
		if (temp_count != cpu_on_count)
			break;

		/* Check whether to suspend before iterating */
		if (include_cpu_suspend) {
			ret = tftf_program_timer_and_suspend(
			PLAT_SUSPEND_ENTRY_TIME, power_state, NULL, NULL);

			tftf_cancel_timer();

			if (ret != PSCI_E_SUCCESS) {
				tftf_testcase_printf("CPU timer/suspend"
					" returned error 0x%x\n", ret);
				return TEST_RESULT_FAIL;
			}
		}
	}

	spin_lock(&counter_lock);
	if (cpu_on_count >= (STRESS_TEST_COUNT * PLATFORM_CORE_COUNT)) {
		cpu_on_count = 0;
		spin_unlock(&counter_lock);
		exit_test = 1;
		/* Wait for all cores to power OFF */
		wait_for_non_lead_cpus();

		/*
		 * In case any other CPUs were turned ON in the meantime, wait
		 * for them as well.
		 */
		wait_for_non_lead_cpus();
	} else
		spin_unlock(&counter_lock);

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Stress test CPU ON / OFF APIs.
 * Test Do :
 * 1. Maintain a global counter will be atomically updated by each CPU
 *    when it turns ON.
 * 2. Iterate over all the CPU in the topology and invoke CPU ON on CPU
 *    which are OFF.
 * 3. Check if the global counter has updated. If not, goto step 2
 * 4. All the cores which are turned ON executes Step 1 - 3.
 * Note: The test will be skipped on single-core platforms.
 */
test_result_t psci_cpu_on_off_stress(void)
{
	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);
	init_spinlock(&counter_lock);
	cpu_on_count = 0;
	exit_test = 0;
	include_cpu_suspend = 0;
	return launch_cpu_on_off_stress();
}

/*
 * @Test_Aim@ Stress test CPU ON / OFF APIs with SUSPEND in between.
 * Test Do :
 * 1. Maintain a global counter which will be atomically updated by each CPU
 *    when it turns ON.
 * 2. Iterate over all the CPUs in the topology and invoke CPU ON on CPUs
 *    which are OFF.
 * 3. Check if the global counter has updated. If not, program wakeup
 *    timer and suspend. On wake-up goto step 2.
 * 4. All the cores which are turned ON executes Step 1 - 3.
 * Note: The test will be skipped on single-core platforms.
 */
test_result_t psci_cpu_on_off_suspend_stress(void)
{
	int rc;
	unsigned int stateid;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	init_spinlock(&counter_lock);
	cpu_on_count = 0;
	exit_test = 0;

	rc = tftf_psci_make_composite_state_id(PLAT_MAX_PWR_LEVEL,
					PSTATE_TYPE_POWERDOWN, &stateid);
	if (rc != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to construct composite state\n");
		return TEST_RESULT_SKIPPED;
	}
	power_state = tftf_make_psci_pstate(PLAT_MAX_PWR_LEVEL,
			PSTATE_TYPE_POWERDOWN, stateid);

	include_cpu_suspend = 1;
	return launch_cpu_on_off_stress();
}
