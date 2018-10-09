/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <events.h>
#include <irq.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <tftf_lib.h>
#include <timer.h>

/*
 * Desired affinity level and state type (standby or powerdown) for the next
 * CPU_SUSPEND operation. We need these shared variables because there is no way
 * to pass arguments to non-lead CPUs...
 */
static unsigned int test_aff_level;
static unsigned int test_suspend_type;

static event_t cpu_ready[PLATFORM_CORE_COUNT];

/*
 * Variable used by the non-lead CPUs to tell the lead CPU they
 * were woken up by IRQ_WAKE_SGI
 */
static event_t event_received_wake_irq[PLATFORM_CORE_COUNT];

/* Variable used to confirm the CPU is woken up by IRQ_WAKE_SGI or Timer IRQ */
static volatile int requested_irq_received[PLATFORM_CORE_COUNT];

static int requested_irq_handler(void *data)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
#if ENABLE_ASSERTIONS
	unsigned int irq_id = *(unsigned int *) data;
#endif

	assert(irq_id == IRQ_WAKE_SGI || irq_id == tftf_get_timer_irq());
	assert(requested_irq_received[core_pos] == 0);

	requested_irq_received[core_pos] = 1;

	return 0;
}

/*
 * Suspend the calling (non-lead) CPU.
 * 1) Program a wake-up event to come out of suspend state
 * 2) Suspend the CPU to the desired affinity level and power state (standby or
 *    powerdown)
 * 3) Report success/failure of the suspend operation
 */
static test_result_t suspend_non_lead_cpu(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);
	uint32_t power_state, stateid;
	int rc, expected_return_val;
	u_register_t flags;

	tftf_timer_register_handler(requested_irq_handler);

	/* Tell the lead CPU that the calling CPU is about to suspend itself */
	tftf_send_event(&cpu_ready[core_pos]);

	/* IRQs need to be disabled prior to programming the timer */
	/* Preserve DAIF flags*/
	flags = read_daif();
	disable_irq();

	rc = tftf_program_timer(PLAT_SUSPEND_ENTRY_TIME);
	if (rc != 0) {
		/* Restore previous DAIF flags */
		write_daif(flags);
		isb();
		ERROR("Timer programming failed with error %d\n", rc);
		return TEST_RESULT_FAIL;
	}

	expected_return_val = tftf_psci_make_composite_state_id(test_aff_level,
				   test_suspend_type, &stateid);

	/*
	 * Suspend the calling CPU to the desired affinity level and power state
	 */
	power_state =  tftf_make_psci_pstate(test_aff_level,
					     test_suspend_type,
					     stateid);
	rc = tftf_cpu_suspend(power_state);

	/* Restore previous DAIF flags */
	write_daif(flags);
	isb();

	/* Wait until the IRQ wake interrupt is received */
	while (!requested_irq_received[core_pos])
		;

	tftf_send_event(&event_received_wake_irq[core_pos]);
	tftf_timer_unregister_handler();

	if (rc == expected_return_val)
		return TEST_RESULT_SUCCESS;

	tftf_testcase_printf("Wrong value: expected %i, got %i\n",
					expected_return_val, rc);
	return TEST_RESULT_FAIL;
}

/*
 * CPU suspend test to the desired affinity level and power state
 *
 * 1) Power on all cores
 * 2) Each core registers a wake-up event to come out of suspend state
 * 3) Each core tries to enter suspend state
 *
 * The test is skipped if an error occurs during the bring-up of non-lead CPUs.
 */
static test_result_t test_psci_suspend(unsigned int aff_level,
				     unsigned int suspend_type)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int target_mpid, target_node;
	unsigned int core_pos;
	uint32_t power_state, stateid;
	int rc, expected_return_val;
	u_register_t flags;

	if (aff_level > MPIDR_MAX_AFFLVL)
		return TEST_RESULT_SKIPPED;

	assert((suspend_type == PSTATE_TYPE_POWERDOWN) ||
	       (suspend_type == PSTATE_TYPE_STANDBY));

	/* Export these variables for the non-lead CPUs */
	test_aff_level = aff_level;
	test_suspend_type = suspend_type;

	/*
	 * All testcases in this file use the same cpu_ready[] array so it needs
	 * to be re-initialised each time.
	 */
	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; ++i) {
		tftf_init_event(&cpu_ready[i]);
		tftf_init_event(&event_received_wake_irq[i]);
		requested_irq_received[i] = 0;
	}
	/* Ensure the above writes are seen before any read */
	dmbsy();

	/*
	 * Preparation step: Power on all cores.
	 */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU as it is already on */
		if (target_mpid == lead_mpid)
			continue;

		rc = tftf_cpu_on(target_mpid,
				(uintptr_t) suspend_non_lead_cpu,
				0);
		if (rc != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU 0x%x (%d)\n",
				target_mpid, rc);
			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for all non-lead CPUs to be ready */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU */
		if (target_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(target_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	/* IRQs need to be disabled prior to programming the timer */
	/* Preserve DAIF flags*/
	flags = read_daif();
	disable_irq();

	/*
	 * Program the timer, this will serve as the
	 * wake-up event to come out of suspend state.
	 */
	rc = tftf_program_timer(PLAT_SUSPEND_ENTRY_TIME);
	if (rc) {
		/* Restore previous DAIF flags */
		write_daif(flags);
		isb();
		ERROR("Timer programming failed with error %d\n", rc);
		return TEST_RESULT_FAIL;
	}

	expected_return_val = tftf_psci_make_composite_state_id(test_aff_level,
				   test_suspend_type, &stateid);

	/*
	 * Suspend the calling CPU to the desired affinity level and power state
	 */
	power_state = tftf_make_psci_pstate(test_aff_level,
					    test_suspend_type,
					    stateid);
	if (test_aff_level >= PSTATE_AFF_LVL_2)
		rc = tftf_cpu_suspend_save_sys_ctx(power_state);
	else
		rc = tftf_cpu_suspend(power_state);

	/* Restore previous DAIF flags */
	write_daif(flags);
	isb();

	/*
	 * Cancel the timer set up by lead CPU in case we have returned early
	 * due to invalid parameters or it will lead to spurious wake-up later.
	 */
	tftf_cancel_timer();

	/*
	 * Wait for all non-lead CPUs to receive IRQ_WAKE_SGI. This will also
	 * ensure that the lead CPU has received the system timer IRQ
	 * because SGI #IRQ_WAKE_SGI is sent only after that.
	 */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU */
		if (target_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(target_mpid);
		tftf_wait_for_event(&event_received_wake_irq[core_pos]);
	}

	if (rc == expected_return_val)
		return TEST_RESULT_SUCCESS;

	tftf_testcase_printf("Wrong value: expected %i, got %i\n",
					expected_return_val, rc);
	return TEST_RESULT_FAIL;
}

/*
 * @Test_Aim@ Suspend to powerdown state targeted at affinity level 0
 */
test_result_t test_psci_suspend_powerdown_level0(void)
{
	return test_psci_suspend(PSTATE_AFF_LVL_0, PSTATE_TYPE_POWERDOWN);
}

/*
 * @Test_Aim@ Suspend to standby state targeted at affinity level 0
 */
test_result_t test_psci_suspend_standby_level0(void)
{
	return test_psci_suspend(PSTATE_AFF_LVL_0, PSTATE_TYPE_STANDBY);
}

/*
 * @Test_Aim@ Suspend to powerdown state targeted at affinity level 1
 */
test_result_t test_psci_suspend_powerdown_level1(void)
{
	return test_psci_suspend(PSTATE_AFF_LVL_1, PSTATE_TYPE_POWERDOWN);
}

/*
 * @Test_Aim@ Suspend to standby state targeted at affinity level 1
 */
test_result_t test_psci_suspend_standby_level1(void)
{
	return test_psci_suspend(PSTATE_AFF_LVL_1, PSTATE_TYPE_STANDBY);
}

/*
 * @Test_Aim@ Suspend to powerdown state targeted at affinity level 2
 */
test_result_t test_psci_suspend_powerdown_level2(void)
{
	return test_psci_suspend(PSTATE_AFF_LVL_2, PSTATE_TYPE_POWERDOWN);
}

/*
 * @Test_Aim@ Suspend to standby state targeted at affinity level 2
 */
test_result_t test_psci_suspend_standby_level2(void)
{
	return test_psci_suspend(PSTATE_AFF_LVL_2, PSTATE_TYPE_STANDBY);
}

/*
 * @Test_Aim@ Suspend to powerdown state targeted at affinity level 3
 */
test_result_t test_psci_suspend_powerdown_level3(void)
{
	return test_psci_suspend(PSTATE_AFF_LVL_3, PSTATE_TYPE_POWERDOWN);
}

/*
 * @Test_Aim@ Suspend to standby state targeted at affinity level 3
 */
test_result_t test_psci_suspend_standby_level3(void)
{
	return test_psci_suspend(PSTATE_AFF_LVL_3, PSTATE_TYPE_STANDBY);
}
