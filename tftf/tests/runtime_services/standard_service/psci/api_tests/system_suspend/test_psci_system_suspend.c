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
#include <test_helpers.h>
#include <tftf.h>
#include <tftf_lib.h>
#include <timer.h>

#define SUSPEND_TIME_3_SECS 3000
#define SUSPEND_TIME_10_SECS 10000
#define TEST_ITERATION_COUNT 0x5

/* TODO: Remove assumption that affinity levels always map to power levels. */
#define MPIDR_CLUSTER_ID(mpid)	MPIDR_AFF_ID(mpid, 1)
#define MPIDR_CPU_ID(mpid)	MPIDR_AFF_ID(mpid, 0)

/* Helper macro to verify if system suspend API is supported */
#define is_psci_sys_susp64_supported()	\
	(tftf_get_psci_feature_info(SMC_PSCI_SYSTEM_SUSPEND64) != \
	PSCI_E_NOT_SUPPORTED)

static unsigned int deepest_power_state;
static unsigned int test_target_node = PWR_DOMAIN_INIT;
static event_t cpu_ready[PLATFORM_CORE_COUNT];
static event_t sgi_received[PLATFORM_CORE_COUNT];
static event_t waitq[PLATFORM_CORE_COUNT];

static volatile int wakeup_irq_rcvd[PLATFORM_CORE_COUNT];
static volatile unsigned int sgi_handled[PLATFORM_CORE_COUNT];
static sgi_data_t sgi_data;
static volatile int cpu_ref_count;

extern unsigned long __RO_START__;
#define TFTF_RO_START (unsigned long)(&__RO_START__)
extern unsigned long __RO_END__;
#define TFTF_RO_END (unsigned long)(&__RO_END__)

static int suspend_wakeup_handler(void *data)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());

	assert(wakeup_irq_rcvd[core_pos] == 0);
	wakeup_irq_rcvd[core_pos] = 1;

	return 0;
}

static int sgi_handler(void *data)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	sgi_data = *(sgi_data_t *) data;
	sgi_handled[core_pos] = 1;
	return 0;
}

/*
 * Iterate over all cores and issue system suspend
 * After returning from suspend, ensure that the core which entered
 * suspend resumed from suspend.
 */
static test_result_t sys_suspend_from_all_cores(void)
{
	unsigned long long my_mpid = read_mpidr_el1() & MPID_MASK, target_mpid;
	unsigned int core_pos = platform_get_core_pos(my_mpid);
	int ret;
	int psci_ret;

	/* Increment the count of CPUs in the test */
	cpu_ref_count++;
	dsbsy();

	while (!is_sys_suspend_state_ready())
		;

	wakeup_irq_rcvd[core_pos] = 0;

	/* Register timer handler */
	tftf_timer_register_handler(suspend_wakeup_handler);

	/* Program timer to fire after delay */
	ret = tftf_program_timer_and_sys_suspend(PLAT_SUSPEND_ENTRY_TIME,
								NULL, NULL);

	/* Wait until the IRQ wake interrupt is received */
	while (!wakeup_irq_rcvd[core_pos])
		;

	if (ret) {
		tftf_testcase_printf("Failed to program timer or suspend "
					"system from core %x\n", core_pos);
		return TEST_RESULT_FAIL;
	}

	/* Unregister time handler */
	tftf_timer_unregister_handler();
	tftf_cancel_timer();

	/* Done with the suspend test. Decrement count */
	cpu_ref_count--;
	dsbsy();

	test_target_node = tftf_topology_next_cpu(test_target_node);
	if (test_target_node != PWR_DOMAIN_INIT) {
		target_mpid = tftf_get_mpidr_from_node(test_target_node);
		psci_ret = tftf_cpu_on(target_mpid,
				(uintptr_t) sys_suspend_from_all_cores,
				0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("Failed to power on CPU 0x%x (%d) \n",
				(unsigned int)target_mpid, psci_ret);
			return TEST_RESULT_FAIL;
		}

		/*
		 * Wait for the target CPU to enter the test. The TFTF framework
		 * requires more than one CPU to be in the test to detect that the
		 * test has not finished.
		 */
		while (!cpu_ref_count)
			;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Functionality test : Issue system suspend from all cores
 * sequentially. This test ensures that system suspend can be issued
 * from all cores and right core is resumed from system suspend.
 */
test_result_t test_system_suspend_from_all_cores(void)
{
	unsigned long long target_mpid, my_mpid;
	int psci_ret;

	test_target_node = PWR_DOMAIN_INIT;
	my_mpid = read_mpidr_el1() & MPID_MASK;

	if (!is_psci_sys_susp64_supported()) {
		tftf_testcase_printf("System suspend is not supported "
				"by the EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; ++i)
		tftf_init_event(&cpu_ready[i]);

	test_target_node = tftf_topology_next_cpu(test_target_node);
	assert(test_target_node != PWR_DOMAIN_INIT);

	target_mpid = tftf_get_mpidr_from_node(test_target_node);
	if (target_mpid == my_mpid)
		return sys_suspend_from_all_cores();

	psci_ret = tftf_cpu_on(target_mpid,
				(uintptr_t) sys_suspend_from_all_cores,
				0);
	if (psci_ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to power on CPU 0x%x (%d) \n",
				(unsigned int)target_mpid, psci_ret);
		return TEST_RESULT_FAIL;
	}

	/*
	 * Wait for the target CPU to enter the test. The TFTF framework
	 * requires more than one CPU to be in the test to detect that the
	 * test has not finished.
	 */
	while (!cpu_ref_count)
		;

	return TEST_RESULT_SUCCESS;
}

/*
 * Helper function to issue SYSTEM SUSPEND SMC with custom parameters.
 */
int sys_suspend_helper(uintptr_t entry_point_address,
		       u_register_t context_id)
{
	smc_args args = {
		SMC_PSCI_SYSTEM_SUSPEND,
		(uintptr_t)entry_point_address,
		(u_register_t)context_id
	};
	smc_ret_values ret_vals;

	ret_vals = tftf_smc(&args);

	return ret_vals.ret0;
}

/*
 * Function to issue system suspend with invalid entry-point on all cores
 * sequentially.
 */
static test_result_t invalid_entrypoint_for_sys_suspend(void)
{
	unsigned long long target_mpid;
	int psci_ret;

	/* Increment the count of CPUs in the test */
	cpu_ref_count++;
	dsbsy();

	while (!is_sys_suspend_state_ready())
		;

	psci_ret = sys_suspend_helper((uintptr_t) 0x1, 0);
	if (psci_ret != PSCI_E_INVALID_ADDRESS) {
		tftf_testcase_printf("Test failed with invalid entry addr %x\n",
				psci_ret);
		return TEST_RESULT_FAIL;
	}

	/* Done with the suspend test. Decrement count */
	cpu_ref_count--;
	dsbsy();

	test_target_node = tftf_topology_next_cpu(test_target_node);
	if (test_target_node != PWR_DOMAIN_INIT) {
		target_mpid = tftf_get_mpidr_from_node(test_target_node);
		psci_ret = tftf_cpu_on(target_mpid,
				(uintptr_t) invalid_entrypoint_for_sys_suspend,
				0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("Failed to power on CPU 0x%x (%d) \n",
				(unsigned int)target_mpid, psci_ret);
			return TEST_RESULT_FAIL;
		}

		/*
		 * Wait for the target CPU to enter the test. The TFTF framework
		 * requires more than one CPU to be in the test to detect that the
		 * test has not finished.
		 */
		while (!cpu_ref_count)
			;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ API test: Issue system suspend with invalid entrypoint on all
 * cores. It should return error.
 */
test_result_t test_system_suspend_invalid_entrypoint(void)
{
	unsigned long long target_mpid, my_mpid;
	int psci_ret;

	test_target_node = PWR_DOMAIN_INIT;
	my_mpid = read_mpidr_el1() & MPID_MASK;

	if (!is_psci_sys_susp64_supported()) {
		tftf_testcase_printf("System suspend is not supported "
				"by the EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; ++i)
		tftf_init_event(&cpu_ready[i]);

	test_target_node = tftf_topology_next_cpu(test_target_node);
	assert(test_target_node != PWR_DOMAIN_INIT);

	target_mpid = tftf_get_mpidr_from_node(test_target_node);
	if (target_mpid == my_mpid)
		return invalid_entrypoint_for_sys_suspend();

	psci_ret = tftf_cpu_on(target_mpid,
				(uintptr_t) invalid_entrypoint_for_sys_suspend,
				0);
	if (psci_ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to power on CPU 0x%x (%d) \n",
				(unsigned int)target_mpid, psci_ret);
		return TEST_RESULT_FAIL;
	}

	/*
	 * Wait for the target CPU to enter the test. The TFTF framework
	 * requires more than one CPU to be in the test to detect that the
	 * test has not finished.
	 */
	while (!cpu_ref_count)
		;

	return TEST_RESULT_SUCCESS;
}

/*
 * Function to test Non lead CPU response to SGIs after multiple invocations
 * of system suspend.
 */
static test_result_t non_lead_cpu_sgi_test(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);
	const unsigned int sgi_id = IRQ_NS_SGI_0;
	int sgi_ret;

	/* Register the local IRQ handler for the SGI */
	sgi_ret = tftf_irq_register_handler(sgi_id, sgi_handler);
	if (sgi_ret != 0) {
		tftf_testcase_printf("Failed to register IRQ %u (%d)",
				sgi_id, sgi_ret);
		return TEST_RESULT_FAIL;
	}
	/* Enable SGI */
	tftf_irq_enable(sgi_id, GIC_HIGHEST_NS_PRIORITY);

	/* Signal to the lead CPU that we are ready to receive SGI */
	tftf_send_event(&cpu_ready[core_pos]);

	/* Wait for SGI */
	while (sgi_handled[core_pos] == 0)
		;
	/* Send event to indicate reception of SGI */
	tftf_send_event(&sgi_received[core_pos]);

	/* Unregister SGI handler */
	tftf_irq_disable(sgi_id);
	tftf_irq_unregister_handler(sgi_id);
	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Functionality test: Issue system suspend multiple times with
 * all non-lead cores as OFF. This test ensures invoking system suspend
 * multiple times on lead core does not have any issue.
 * Steps:
 *    - Register timer wakeup event and issue system suspend multiple
 *      times. Ensure that the system suspend succeeds.
 *    - Turn on all the non lead CPU and send SGIs to them to ensure that
 *      all the non lead CPU are responsive.
 */
test_result_t test_psci_sys_susp_multiple_iteration(void)
{
	unsigned int target_mpid, target_node;
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(lead_mpid);
	const unsigned int sgi_id = IRQ_NS_SGI_0;
	int psci_ret;
	int timer_ret;

	if (!is_psci_sys_susp64_supported()) {
		tftf_testcase_printf("System suspend is not supported "
				"by the EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; ++i) {
		tftf_init_event(&cpu_ready[i]);
		tftf_init_event(&sgi_received[i]);
	}

	/* Register timer handler */
	tftf_timer_register_handler(suspend_wakeup_handler);

	for (unsigned int i = 0; i < TEST_ITERATION_COUNT; i++) {
		wakeup_irq_rcvd[core_pos] = 0;

		/*
		 * Program the wakeup timer, this will serve as the wake-up event
		 * to come out of suspend state, and issue system suspend
		 */
		tftf_program_timer_and_sys_suspend(
				PLAT_SUSPEND_ENTRY_TIME, &timer_ret, &psci_ret);

		while (!wakeup_irq_rcvd[core_pos])
			;

		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("System suspend failed with return value %i\n",
								psci_ret);
			return TEST_RESULT_FAIL;
		}
		if (timer_ret) {
			tftf_testcase_printf("Timer programming failed with return value %i\n",
								timer_ret);
			return TEST_RESULT_FAIL;
		}
	}

	tftf_cancel_timer();
	/* Unregister timer handler */
	tftf_timer_unregister_handler();

	/* Turn on all cores after test to ensure all cores boot up*/
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		core_pos = platform_get_core_pos(target_mpid);

		if (target_mpid == lead_mpid)
			continue;

		psci_ret = tftf_cpu_on(target_mpid,
				(uintptr_t) non_lead_cpu_sgi_test,
				0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
					"Failed to power on CPU 0x%x (%d)\n",
					target_mpid, psci_ret);
			return TEST_RESULT_FAIL;
		}
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	/* Send SGI to all non lead CPUs and ensure that they receive it */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU */
		if (target_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(target_mpid);
		tftf_send_sgi(sgi_id, core_pos);
		tftf_wait_for_event(&sgi_received[core_pos]);
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Functionality test : Issue system suspend with pending
 * SGI on calling core. System suspend call should return prior to the
 * programmed wake-up interval.
 * Steps:
 *   - Mask the interrupts on lead CPU and send SGI to current CPU
 *   - Configure a wake-up timer and issue SYSTEM SUSPEND
 *   - Unmask the interrupt and verify that current CPU has woken
 *     prior to the wake-up timer firing.
 */
test_result_t test_psci_sys_susp_pending_irq(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	const unsigned int sgi_id = IRQ_NS_SGI_0;
	int sgi_ret;
	int psci_ret;
	test_result_t ret = TEST_RESULT_SUCCESS;

	if (!is_psci_sys_susp64_supported()) {
		tftf_testcase_printf("System suspend is not supported "
				"by the EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	/* Initialize variables */
	sgi_handled[core_pos] = 0;
	wakeup_irq_rcvd[core_pos] = 0;

	/* Register the local IRQ handler for the SGI */
	sgi_ret = tftf_irq_register_handler(sgi_id, sgi_handler);
	if (sgi_ret != 0) {
		tftf_testcase_printf("Failed to register IRQ %u (%d)",
				sgi_id, sgi_ret);
		return TEST_RESULT_FAIL;
	}

	/* Register for timer interrupt */
	tftf_timer_register_handler(suspend_wakeup_handler);

	/*
	 * Program the MB timer, for 3 secs to fire timer interrupt if
	 * system enters suspend state with pending IRQ
	 */
	tftf_program_timer(SUSPEND_TIME_3_SECS);

	tftf_irq_enable(sgi_id, GIC_HIGHEST_NS_PRIORITY);
	disable_irq();

	/* Send the SGI to the lead CPU */
	tftf_send_sgi(sgi_id, core_pos);

	/* Check if system enters suspend state with pending IRQ or not */
	psci_ret = tftf_system_suspend();

	/* Un-mask the interrupt */
	enable_irq();

	/*
	 * If the wake-up timer has fired, then the pending interrupt did
	 * not have any effect on the SYSTEM SUSPEND which means the
	 * test case failed.
	 *
	 */
	if (wakeup_irq_rcvd[core_pos]) {
		tftf_testcase_printf("Timer irq received\n");
		ret = TEST_RESULT_FAIL;
	}

	/* Wait for the SGI to be handled */
	while (sgi_handled[core_pos] == 0)
		;

	/* Verify the sgi data received by the SGI handler */
	if (sgi_data.irq_id != sgi_id) {
		tftf_testcase_printf("Wrong IRQ ID, expected %u, got %u\n",
				sgi_id, sgi_data.irq_id);
		ret = TEST_RESULT_FAIL;
	}

	if (psci_ret != PSCI_E_SUCCESS)
		ret = TEST_RESULT_FAIL;

	/* Unregister timer handler */
	tftf_timer_unregister_handler();
	tftf_cancel_timer();

	/* Unregister SGI handler */
	tftf_irq_disable(sgi_id);
	tftf_irq_unregister_handler(sgi_id);

	return ret;
}

/* Helper function to calculate checksum of a given DRAM area  */
unsigned long check_data_integrity(unsigned int *addr, unsigned int size)
{
	unsigned int chksum = 0;
	unsigned int i;

	for (i = 0; i < (size/sizeof(unsigned int)); i++)
		chksum += *(addr + i);
	return chksum;
}

/*
 * @Test_Aim@ Functionality Test: Ensure that RAM contents are preserved on
 * resume from system suspend
 * Steps:
 *    - Write a known pattern to the DRAM and calculate the hash.
 *    - Configure wake-up timer and issue SYSTEM SUSPEND on lead CPU.
 *    - Recalculate the hash of the DRAM and compare it with the previous
 *      value. Both the hash values should match.
 *
 */
test_result_t test_psci_sys_susp_validate_ram(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned long prev_hash_val = 0;
	unsigned long present_hash_val = 0;
	int psci_ret;
	int timer_ret;

	test_result_t ret = TEST_RESULT_SUCCESS;

	if (!is_psci_sys_susp64_supported()) {
		tftf_testcase_printf("System suspend is not supported "
				"by the EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	wakeup_irq_rcvd[core_pos] = 0;

	/* Check hash on known region of RAM before putting into suspend */
	prev_hash_val = check_data_integrity((unsigned int *)TFTF_RO_START,
						TFTF_RO_END - TFTF_RO_START);

	tftf_timer_register_handler(suspend_wakeup_handler);

	/*
	 * Program timer to fire interrupt after timer expires and issue
	 * system suspend
	 */
	tftf_program_timer_and_sys_suspend(SUSPEND_TIME_10_SECS,
						     &timer_ret, &psci_ret);

	while (!wakeup_irq_rcvd[core_pos])
		;
	if (psci_ret == PSCI_E_SUCCESS) {
		/*
		 * Check hash on known region of RAM after returning
		 * from suspend
		 */
		present_hash_val = check_data_integrity(
				(unsigned int *)TFTF_RO_START,
				TFTF_RO_END - TFTF_RO_START);
		if (present_hash_val != prev_hash_val) {
			tftf_testcase_printf("ERROR: RAM data not retained \n");
			ret = TEST_RESULT_FAIL;
		}
	} else {
		tftf_testcase_printf("Failed: system suspend to RAM \n");
		ret = TEST_RESULT_FAIL;
	}

	if (timer_ret) {
		tftf_testcase_printf("Failed: timer programming \n");
		ret = TEST_RESULT_FAIL;
	}

	/* Unregister timer handler */
	tftf_timer_unregister_handler();
	tftf_cancel_timer();

	return ret;
}

/* Helper function to get deepest power state */
static unsigned int get_deepest_power_state(void)
{
	unsigned int test_suspend_type;
	unsigned int suspend_state_id;
	unsigned int power_level;
	unsigned int power_state = 0;
	unsigned int pstate_id_idx[PLAT_MAX_PWR_LEVEL + 1];
	int ret;

	INIT_PWR_LEVEL_INDEX(pstate_id_idx);
	do {
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, pstate_id_idx);

		if (pstate_id_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		ret = tftf_get_pstate_vars(&power_level,
				&test_suspend_type,
				&suspend_state_id,
				pstate_id_idx);
		if (ret)
			continue;

		power_state = tftf_make_psci_pstate(power_level,
				test_suspend_type,
				suspend_state_id);

	} while (1);

	return power_state;
}

/*
 * Suspend non-lead cores
 */
static test_result_t suspend_non_lead_cpu(void)
{
	unsigned long long mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);
	int ret;

	tftf_irq_enable(IRQ_NS_SGI_0, GIC_HIGHEST_NS_PRIORITY);

	/* Tell the lead CPU that the calling CPU is about to suspend itself */
	tftf_send_event(&cpu_ready[core_pos]);

	ret = tftf_cpu_suspend(deepest_power_state);
	tftf_irq_disable(IRQ_NS_SGI_0);

	if (ret) {
		ERROR(" CPU suspend failed with error %x\n", ret);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ API Test: Issue system suspend on a core while other
 * cores are in suspend. This test ensures that system suspend will
 * not be successful if cores other than core issuing suspend are not
 * in OFF state.
 * Steps:
 *    - Turn on non lead CPUs and suspend it to the deepest suspend
 *      power state.
 *    - Issue SYSTEM SUSPEND on primary CPU. The API should return
 *      error.
 *
 */
test_result_t test_psci_sys_susp_with_cores_in_suspend(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned int target_mpid, target_node;
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	int psci_ret;
	int timer_ret;
	test_result_t ret = TEST_RESULT_SUCCESS;

	if (!is_psci_sys_susp64_supported()) {
		tftf_testcase_printf("System suspend is not supported "
				"by the EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	SKIP_TEST_IF_LESS_THAN_N_CLUSTERS(2);

	for (unsigned int j = 0; j < PLATFORM_CORE_COUNT; j++)
		tftf_init_event(&cpu_ready[j]);

	wakeup_irq_rcvd[core_pos] = 0;
	deepest_power_state = get_deepest_power_state();

	/* Suspend all cores other than lead core */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);

		if (target_mpid == lead_mpid)
			continue;

		/* Turn on the non lead CPU and suspend it. */
		psci_ret = tftf_cpu_on(target_mpid,
				(uintptr_t) suspend_non_lead_cpu,
				0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
					"Failed to power on CPU 0x%x (%d)\n",
					(unsigned int)target_mpid, psci_ret);
			return TEST_RESULT_FAIL;
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

	/* Wait for 10 ms to ensure all the secondaries have suspended */
	waitms(10);

	/*
	 * Register and program timer, then issue a system suspend
	 * when other cores are in suspend state
	 */
	tftf_timer_register_handler(suspend_wakeup_handler);
	tftf_program_timer_and_sys_suspend(
		PLAT_SUSPEND_ENTRY_TIME, &timer_ret, &psci_ret);

	/* Wake all non-lead CPUs */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU */
		if (target_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(target_mpid);
		tftf_send_sgi(IRQ_NS_SGI_0, core_pos);
	}

	/* Check return from value from system suspend API */
	if (psci_ret != PSCI_E_DENIED) {
		tftf_testcase_printf("Entered suspend with cores in suspend\n");
		ret = TEST_RESULT_FAIL;
	}
	if (timer_ret) {
		tftf_testcase_printf("Failed to program the timer\n");
		ret = TEST_RESULT_FAIL;
	}
	/* Unregister and cancel timer */
	tftf_timer_unregister_handler();
	tftf_cancel_timer();

	return ret;
}

/*
 * This functions holds the CPU till a `waitq` event is received.
 */
static test_result_t cpu_waitq(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);

	tftf_send_event(&cpu_ready[core_pos]);

	/* Wait for event from primary cpu */
	tftf_wait_for_event(&waitq[core_pos]);
	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ API TEST: Ensure that system suspend will not be successful
 * if cores other than core issuing suspend are in running state
 *
 * Steps :
 *    - Turn on multiple cores on the non lead cluster
 *    - Issue SYSTEM SUSPEND. The API should return error.
 */
test_result_t test_psci_sys_susp_with_cores_on(void)
{
	unsigned int lead_cluster = MPIDR_CLUSTER_ID(read_mpidr_el1());
	unsigned int core_pos;
	unsigned int target_mpid, target_node;
	int psci_ret;
	int timer_ret;
	test_result_t ret = TEST_RESULT_SUCCESS;

	if (!is_psci_sys_susp64_supported()) {
		tftf_testcase_printf("System suspend is not supported "
				"by the EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	SKIP_TEST_IF_LESS_THAN_N_CLUSTERS(2);

	for (unsigned int j = 0; j < PLATFORM_CORE_COUNT; j++) {
		tftf_init_event(&waitq[j]);
		tftf_init_event(&cpu_ready[j]);
		wakeup_irq_rcvd[j] = 0;
	}

	/* Turn on cores in non-lead cluster */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);

		if (MPIDR_CLUSTER_ID(target_mpid) == lead_cluster)
			continue;

		psci_ret = tftf_cpu_on(target_mpid,
				(uintptr_t) cpu_waitq,
				0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("Failed to power "
				"on CPU 0x%x (%d)\n",
				(unsigned int)target_mpid, psci_ret);
			return TEST_RESULT_FAIL;
		}

		core_pos = platform_get_core_pos(target_mpid);
		/* Ensure that the core has booted */
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	/* Register timer handler */
	tftf_timer_register_handler(suspend_wakeup_handler);

	/*
	 * Program timer to fire after delay and issue system suspend with
	 * other cores in ON state
	 */
	tftf_program_timer_and_sys_suspend(PLAT_SUSPEND_ENTRY_TIME,
							&timer_ret, &psci_ret);

	/* Send event to CPUs waiting for `waitq` event. */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);

		/* Skip lead cluster */
		if (MPIDR_CLUSTER_ID(target_mpid) == lead_cluster)
			continue;

		core_pos = platform_get_core_pos(target_mpid);
		tftf_send_event(&waitq[core_pos]);
	}

	/* Check return value from system suspend API */
	if (psci_ret != PSCI_E_DENIED) {
		tftf_testcase_printf("Test failed when suspending with return "
				     "value: %x \n", psci_ret);
		ret = TEST_RESULT_FAIL;
	}
	if (timer_ret) {
		tftf_testcase_printf("Test failed with return value when "
				     "programming the timer: %x \n", timer_ret);
		ret = TEST_RESULT_FAIL;
	}
	tftf_timer_unregister_handler();
	tftf_cancel_timer();
	return ret;
}
