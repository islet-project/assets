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
#include <mmio.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <stdlib.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <timer.h>

static event_t cpu_ready[PLATFORM_CORE_COUNT];

/* Used to confirm the CPU is woken up by IRQ_WAKE_SGI or Timer IRQ */
static volatile int requested_irq_received[PLATFORM_CORE_COUNT];
/* Used to count number of CPUs woken up by IRQ_WAKE_SGI */
static int multiple_timer_count;
/* Used to count number of CPUs woken up by Timer IRQ */
static int timer_switch_count;
/* Timer step value of a platform */
static unsigned int timer_step_value;
/* Used to program the interrupt time */
static unsigned long long next_int_time;
/* Lock to prevent concurrently modifying next_int_time */
static spinlock_t int_timer_access_lock;
/* Lock to prevent concurrently modifying irq handler data structures */
static spinlock_t irq_handler_lock;

/* Variable to confirm all cores are inside the testcase */
static volatile unsigned int all_cores_inside_test;

/*
 * Used by test cases to confirm if the programmed timer is fired. It also
 * keeps track of how many timer irq's are received.
 */
static int requested_irq_handler(void *data)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned int irq_id = *(unsigned int *) data;

	assert(irq_id == IRQ_WAKE_SGI || irq_id == tftf_get_timer_irq());
	assert(requested_irq_received[core_pos] == 0);

	if (irq_id == tftf_get_timer_irq()) {
		spin_lock(&irq_handler_lock);
		timer_switch_count++;
		spin_unlock(&irq_handler_lock);
	}

	requested_irq_received[core_pos] = 1;

	return 0;
}

/*
 * Used by test cases to confirm if the programmed timer is fired. It also
 * keeps track of how many WAKE_SGI's are received.
 */
static int multiple_timer_handler(void *data)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned int irq_id = *(unsigned int *) data;

	assert(irq_id == IRQ_WAKE_SGI || irq_id == tftf_get_timer_irq());
	assert(requested_irq_received[core_pos] == 0);

	if (irq_id == IRQ_WAKE_SGI) {
		spin_lock(&irq_handler_lock);
		multiple_timer_count++;
		spin_unlock(&irq_handler_lock);
	}

	requested_irq_received[core_pos] = 1;

	return 0;
}

/*
 * @Test_Aim@ Validates timer interrupt framework and platform timer driver for
 * generation and routing of interrupt to a powered on core.
 *
 * Returns SUCCESS or waits forever in wfi()
 */
test_result_t test_timer_framework_interrupt(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	int ret;

	/* Initialise common variable across tests */
	requested_irq_received[core_pos] = 0;

	/* Register timer handler to confirm it received the timer interrupt */
	ret = tftf_timer_register_handler(requested_irq_handler);
	if (ret != 0) {
		tftf_testcase_printf("Failed to register timer handler:0x%x\n", ret);
		return TEST_RESULT_FAIL;
	}

	ret = tftf_program_timer(tftf_get_timer_step_value() + 1);
	if (ret != 0) {
		tftf_testcase_printf("Failed to program timer:0x%x\n", ret);
		return TEST_RESULT_FAIL;
	}
	wfi();

	while (!requested_irq_received[core_pos])
		;
	ret = tftf_timer_unregister_handler();
	if (ret != 0) {
		tftf_testcase_printf("Failed to unregister timer handler:0x%x\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	return TEST_RESULT_SUCCESS;
}

static test_result_t timer_target_power_down_cpu(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned int power_state;
	unsigned int stateid;
	int ret;
	unsigned long timer_delay;

	tftf_send_event(&cpu_ready[core_pos]);
	/* Initialise common variable across tests */
	requested_irq_received[core_pos] = 0;

	/* Construct the state-id for power down */
	ret = tftf_psci_make_composite_state_id(MPIDR_AFFLVL0,
					PSTATE_TYPE_POWERDOWN, &stateid);
	if (ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to construct composite state\n");
		return TEST_RESULT_FAIL;
	}

	/* Register timer handler to confirm it received the timer interrupt */
	ret = tftf_timer_register_handler(requested_irq_handler);
	if (ret != 0) {
		tftf_testcase_printf("Failed to register timer handler:0x%x\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* Wait for all cores to be up */
	while (!all_cores_inside_test)
		;

	power_state = tftf_make_psci_pstate(MPIDR_AFFLVL0,
					    PSTATE_TYPE_POWERDOWN, stateid);

	spin_lock(&int_timer_access_lock);
	timer_delay = PLAT_SUSPEND_ENTRY_TIME + next_int_time;
	next_int_time -= 2 * (timer_step_value + PLAT_SUSPEND_ENTRY_EXIT_TIME);
	spin_unlock(&int_timer_access_lock);

	ret = tftf_program_timer_and_suspend(timer_delay, power_state,
								NULL, NULL);
	if (ret != 0) {
		tftf_testcase_printf(
			"Failed to program timer or suspend CPU: 0x%x\n", ret);
		return TEST_RESULT_FAIL;
	}

	while (!requested_irq_received[core_pos])
		;
	ret = tftf_timer_unregister_handler();
	if (ret != 0) {
		tftf_testcase_printf("Failed to unregister timer handler:0x%x\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Validates routing of timer interrupt to the lowest requested
 * timer interrupt core on power down.
 *
 * Power up all the cores and each requests a timer interrupt lesser than
 * the previous requested core by timer step value. Doing this ensures
 * at least some cores would be waken by Time IRQ.
 *
 * Returns SUCCESS if all cores power up on getting the interrupt.
 */
test_result_t test_timer_target_power_down_cpu(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_mpid, cpu_node;
	unsigned int core_pos;
	unsigned int rc;
	unsigned int valid_cpu_count;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		tftf_init_event(&cpu_ready[i]);
		requested_irq_received[i] = 0;
	}

	if (!timer_step_value)
		timer_step_value =  tftf_get_timer_step_value();

	timer_switch_count = 0;
	all_cores_inside_test = 0;

	/*
	 * To be sure none of the CPUs do not fall in an atomic slice,
	 * all CPU's program the timer as close as possible with a time
	 * difference of twice the sum of step value and suspend entry
	 * exit time.
	 */
	next_int_time = 2 * (timer_step_value + PLAT_SUSPEND_ENTRY_EXIT_TIME) * (PLATFORM_CORE_COUNT + 2);

	/*
	 * Preparation step: Power on all cores.
	 */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already on */
		if (cpu_mpid == lead_mpid)
			continue;

		rc = tftf_cpu_on(cpu_mpid,
				(uintptr_t) timer_target_power_down_cpu,
				0);
		if (rc != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
			"Failed to power on CPU 0x%x (%d)\n",
			cpu_mpid, rc);
			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for all non-lead CPUs to be ready */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	all_cores_inside_test = 1;

	rc = timer_target_power_down_cpu();

	valid_cpu_count = 0;
	/* Wait for all cores to complete the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		core_pos = platform_get_core_pos(cpu_mpid);
		while (!requested_irq_received[core_pos])
			;
		valid_cpu_count++;
	}

	if (timer_switch_count != valid_cpu_count) {
		tftf_testcase_printf("Expected timer switch: %d Actual: %d\n",
					valid_cpu_count, timer_switch_count);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

static test_result_t timer_same_interval(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned int power_state;
	unsigned int stateid;
	int ret;

	tftf_send_event(&cpu_ready[core_pos]);

	/* Initialise common variable across tests */
	requested_irq_received[core_pos] = 0;

	/* Construct the state-id for power down */
	ret = tftf_psci_make_composite_state_id(MPIDR_AFFLVL0,
					PSTATE_TYPE_POWERDOWN, &stateid);
	if (ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to construct composite state\n");
		return TEST_RESULT_FAIL;
	}

	/* Register timer handler to confirm it received the timer interrupt */
	ret = tftf_timer_register_handler(multiple_timer_handler);
	if (ret != 0) {
		tftf_testcase_printf("Failed to register timer handler:0x%x\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* Wait for all cores to be up */
	while (!all_cores_inside_test)
		;

	/*
	 * Lets hope with in Suspend entry time + 10ms, at least some of the CPU's
	 * have same interval
	 */
	power_state = tftf_make_psci_pstate(MPIDR_AFFLVL0,
					    PSTATE_TYPE_POWERDOWN, stateid);
	ret = tftf_program_timer_and_suspend(PLAT_SUSPEND_ENTRY_TIME + 10,
					     power_state, NULL, NULL);
	if (ret != 0) {
		tftf_testcase_printf(
			"Failed to program timer or suspend CPU: 0x%x\n", ret);
	}

	while (!requested_irq_received[core_pos])
		;
	ret = tftf_timer_unregister_handler();
	if (ret != 0) {
		tftf_testcase_printf("Failed to unregister timer handler:0x%x\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Validates routing of timer interrupt when multiple cores
 * requested same time.
 *
 * Power up all the cores and each core requests same time.
 *
 * Returns SUCCESS if all cores get an interrupt and power up.
 */
test_result_t test_timer_target_multiple_same_interval(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_mpid, cpu_node;
	unsigned int core_pos;
	unsigned int rc;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		tftf_init_event(&cpu_ready[i]);
		requested_irq_received[i] = 0;
	}

	multiple_timer_count = 0;
	all_cores_inside_test = 0;

	/*
	 * Preparation step: Power on all cores.
	 */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already on */
		if (cpu_mpid == lead_mpid)
			continue;

		rc = tftf_cpu_on(cpu_mpid,
				(uintptr_t) timer_same_interval,
				0);
		if (rc != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
			"Failed to power on CPU 0x%x (%d)\n",
			cpu_mpid, rc);
			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for all non-lead CPUs to be ready */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	core_pos = platform_get_core_pos(lead_mpid);
	/* Initialise common variable across tests */
	requested_irq_received[core_pos] = 0;

	all_cores_inside_test = 1;

	rc = timer_same_interval();

	/* Wait for all cores to complete the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		core_pos = platform_get_core_pos(cpu_mpid);
		while (!requested_irq_received[core_pos])
			;
	}

	if (rc != TEST_RESULT_SUCCESS)
		return rc;

	/* At least 2 CPUs requests should fall in same timer period. */
	return multiple_timer_count ? TEST_RESULT_SUCCESS : TEST_RESULT_SKIPPED;
}

static test_result_t do_stress_test(void)
{
	unsigned int power_state;
	unsigned int stateid;
	unsigned int timer_int_interval;
	unsigned int verify_cancel;
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned long long end_time;
	unsigned long long current_time;
	int ret;

	tftf_send_event(&cpu_ready[core_pos]);

	end_time = mmio_read_64(SYS_CNT_BASE1 + CNTPCT_LO) + read_cntfrq_el0() * 10;

	/* Construct the state-id for power down */
	ret = tftf_psci_make_composite_state_id(MPIDR_AFFLVL0,
					PSTATE_TYPE_POWERDOWN, &stateid);
	if (ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to construct composite state\n");
		return TEST_RESULT_FAIL;
	}

	/* Register a handler to confirm its woken by programmed interrupt */
	ret = tftf_timer_register_handler(requested_irq_handler);
	if (ret != 0) {
		tftf_testcase_printf("Failed to register timer handler:0x%x\n", ret);
		return TEST_RESULT_FAIL;
	}

	do {
		current_time = mmio_read_64(SYS_CNT_BASE1 + CNTPCT_LO);
		if (current_time > end_time)
			break;

		timer_int_interval = 1 + rand() % 5;
		verify_cancel = rand() % 5;

		requested_irq_received[core_pos] = 0;

		/*
		 * If verify_cancel == 0, cancel the programmed timer. As it can
		 * take values from 0 to 4, we will be cancelling only 20% of
		 * times.
		 */
		if (!verify_cancel) {
			ret = tftf_program_timer(PLAT_SUSPEND_ENTRY_TIME +
						 timer_int_interval);
			if (ret != 0) {
				tftf_testcase_printf("Failed to program timer: "
						     "0x%x\n", ret);
				return TEST_RESULT_FAIL;
			}

			ret = tftf_cancel_timer();
			if (ret != 0) {
				tftf_testcase_printf("Failed to cancel timer: "
						     "0x%x\n", ret);
				return TEST_RESULT_FAIL;
			}
		} else {
			power_state = tftf_make_psci_pstate(
					MPIDR_AFFLVL0, PSTATE_TYPE_POWERDOWN,
					stateid);

			ret = tftf_program_timer_and_suspend(
				PLAT_SUSPEND_ENTRY_TIME + timer_int_interval,
				power_state, NULL, NULL);
			if (ret != 0) {
				tftf_testcase_printf("Failed to program timer "
						     "or suspend: 0x%x\n", ret);
				return TEST_RESULT_FAIL;
			}

			if (!requested_irq_received[core_pos]) {
				/*
				 * Cancel the interrupt as the CPU has been
				 * woken by some other interrupt
				 */
				ret = tftf_cancel_timer();
				if (ret != 0) {
					tftf_testcase_printf("Failed to cancel timer:0x%x\n", ret);
					return TEST_RESULT_FAIL;
				}
			}
		}
	} while (1);

	ret = tftf_timer_unregister_handler();
	if (ret != 0) {
		tftf_testcase_printf("Failed to unregister timer handler:0x%x\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Stress tests timer framework by requesting combination of
 * timer requests with SUSPEND and cancel calls.
 *
 * Returns SUCCESS if all the cores successfully wakes up from suspend
 * and returns back to the framework.
 */
test_result_t stress_test_timer_framework(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_mpid, cpu_node;
	unsigned int core_pos;
	unsigned int rc;

	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		tftf_init_event(&cpu_ready[i]);
		requested_irq_received[i] = 0;
	}
	/*
	 * Preparation step: Power on all cores.
	 */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already on */
		if (cpu_mpid == lead_mpid)
			continue;

		rc = tftf_cpu_on(cpu_mpid,
				(uintptr_t) do_stress_test,
				0);
		if (rc != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
			"Failed to power on CPU 0x%x (%d)\n",
			cpu_mpid, rc);
			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for all non-lead CPUs to be ready */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	return do_stress_test();
}
