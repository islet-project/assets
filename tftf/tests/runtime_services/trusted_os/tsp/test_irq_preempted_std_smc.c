/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <arm_gic.h>
#include <debug.h>
#include <events.h>
#include <gic_common.h>
#include <gic_v2.h>
#include <irq.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <sgi.h>
#include <smccc.h>
#include <string.h>
#include <test_helpers.h>
#include <tftf_lib.h>

#define TEST_ITERATIONS_COUNT	1000

#define SUSPEND_TIME_1_SEC	1000

#define TEST_VALUE_1	4
#define TEST_VALUE_2	6

static event_t cpu_has_entered_test[PLATFORM_CORE_COUNT];
static event_t cpu_has_finished_test[PLATFORM_CORE_COUNT];

static volatile int requested_irq_received[PLATFORM_CORE_COUNT];
static volatile int wakeup_irq_received[PLATFORM_CORE_COUNT];
static volatile int individual_test_failed[PLATFORM_CORE_COUNT];
static volatile int pwr_level_being_tested;
static volatile int test_finished_flag;

/* Dummy timer handler that sets a flag to check it has been called. */
static int suspend_wakeup_handler(void *data)
{
	u_register_t core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);

	assert(wakeup_irq_received[core_pos] == 0);

	wakeup_irq_received[core_pos] = 1;

	return 0;
}

/* Dummy handler that sets a flag so as to check it has been called. */
static int test_handler(void *data)
{
	u_register_t core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);

	assert(requested_irq_received[core_pos] == 0);

	requested_irq_received[core_pos] = 1;

	return 0;
}

/* Register a dummy handler for SGI #0 and enable it. Returns 0 if success. */
static int register_and_enable_test_sgi_handler(unsigned int core_pos)
{
	/* SGIs #0 - #6 are freely available. */

	int ret = tftf_irq_register_handler(IRQ_NS_SGI_0, test_handler);

	if (ret != 0) {
		tftf_testcase_printf(
			"Failed to register SGI handler @ CPU %d (rc = %d)\n",
			core_pos, ret);
		return -1;
	}

	tftf_irq_enable(IRQ_NS_SGI_0, GIC_HIGHEST_NS_PRIORITY);

	return 0;
}

/* Disable and unregister the dummy handler for SGI #0. */
static void unregister_and_disable_test_sgi_handler(void)
{
	tftf_irq_disable(IRQ_NS_SGI_0);

	tftf_irq_unregister_handler(IRQ_NS_SGI_0);
}

/*
 * Generate a pre-empted STD SMC on the CPU who called this function. Steps:
 * 1. IRQs are disabled.
 * 2. An SGI is sent to itself. It cannot be handled because IRQs are disabled.
 * 3. Invoke an STD SMC on the TSP, which is preempted by the pending SGI.
 * 4. IRQs are enabled, the SGI is handled.
 * 5. This function is exited with a preempted STD SMC waiting to be resumed.
 */
static int preempt_std_smc_on_this_cpu(void)
{
	smc_args std_smc_args;
	smc_ret_values smc_ret;

	int result = TEST_RESULT_SUCCESS;
	u_register_t core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);

	if (register_and_enable_test_sgi_handler(core_pos) != 0) {
		return TEST_RESULT_FAIL;
	}

	/* Set PSTATE.I to 0. */
	disable_irq();

	/*
	 * Send SGI to itself. It can't be handled because the
	 * interrupts are disabled.
	 */
	requested_irq_received[core_pos] = 0;

	tftf_send_sgi(IRQ_NS_SGI_0, core_pos);

	/*
	 * Invoke an STD SMC. Should be pre-empted because of the SGI
	 * that is waiting.
	 */
	std_smc_args.fid = TSP_STD_FID(TSP_ADD);
	std_smc_args.arg1 = TEST_VALUE_1;
	std_smc_args.arg2 = TEST_VALUE_2;
	smc_ret = tftf_smc(&std_smc_args);
	if (smc_ret.ret0 != TSP_SMC_PREEMPTED) {
		tftf_testcase_printf("SMC @ CPU %d returned 0x%llX.\n", core_pos,
				     (unsigned long long)smc_ret.ret0);
		result = TEST_RESULT_FAIL;
	}

	/* Set PSTATE.I to 1. Let the SGI be handled. */
	enable_irq();

	/* Cleanup. Disable and unregister SGI handler. */
	unregister_and_disable_test_sgi_handler();

	/*
	 * Check that the SGI has been handled, but don't fail if it hasn't
	 * because there is no guarantee that it will have actually happened at
	 * this point.
	 */
	if (requested_irq_received[core_pos] == 0) {
		VERBOSE("SGI not handled @ CPU %d\n", core_pos);
	}

	return result;
}

/* Resume a pre-empted STD SMC on the CPU who called this function. */
static int resume_std_smc_on_this_cpu(void)
{
	smc_args std_smc_args;
	smc_ret_values smc_ret;

	u_register_t core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);

	/* Resume the STD SMC. Verify result. */
	std_smc_args.fid = TSP_FID_RESUME;
	smc_ret = tftf_smc(&std_smc_args);
	if ((smc_ret.ret0 != 0) || (smc_ret.ret1 != TEST_VALUE_1 * 2)
	    || (smc_ret.ret2 != TEST_VALUE_2 * 2)) {
		tftf_testcase_printf(
			"SMC @ CPU %d returned 0x%llX 0x%llX 0x%llX instead of 0x0 0x%X 0x%X\n",
			core_pos, (unsigned long long)smc_ret.ret0,
			(unsigned long long)smc_ret.ret1,
			(unsigned long long)smc_ret.ret2,
			TEST_VALUE_1 * 2, TEST_VALUE_2 * 2);
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}

/*
 * Try to resume a pre-empted STD SMC on the CPU who called this function,
 * but check for SMC_UNKNOWN as a result.
 */
static int resume_fail_std_smc_on_this_cpu(void)
{
	smc_args std_smc_args;
	smc_ret_values smc_ret;

	u_register_t core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);

	/* Resume the STD SMC. Verify result. */
	std_smc_args.fid = TSP_FID_RESUME;
	smc_ret = tftf_smc(&std_smc_args);
	if (smc_ret.ret0 != SMC_UNKNOWN) {
		tftf_testcase_printf(
			"SMC @ CPU %d returned 0x%llX 0x%llX 0x%llX instead of SMC_UNKNOWN\n",
			core_pos, (unsigned long long)smc_ret.ret0,
			(unsigned long long)smc_ret.ret1,
			(unsigned long long)smc_ret.ret2);
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}

/*******************************************************************************
 * Test pre-emption during STD SMCs.
 ******************************************************************************/

/* Test routine for test_irq_preempted_std_smc. */
static test_result_t test_irq_preempted_std_smc_fn(void)
{
	u_register_t cpu_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(cpu_mpid);

	tftf_send_event(&cpu_has_entered_test[core_pos]);

	for (unsigned int i = 0; i < TEST_ITERATIONS_COUNT; i++) {

		if (preempt_std_smc_on_this_cpu() != TEST_RESULT_SUCCESS)
			return TEST_RESULT_FAIL;

		if (resume_std_smc_on_this_cpu() != TEST_RESULT_SUCCESS)
			return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Multicore preemption test. Tests IRQ preemption during STD SMC
 * from multiple cores. Uses an SGI to trigger the preemption. TSP should be
 * present.
 *
 * Steps: 1. Invoke Standard SMC on the TSP and try to preempt it via IRQ.
 *        2. Resume the preempted SMC and verify the result.
 *
 * Returns SUCCESS if above 2 steps are performed correctly in every CPU else
 * failure.
 */
test_result_t test_irq_preempted_std_smc(void)
{
	u_register_t cpu_mpid;
	unsigned int cpu_node, core_pos;
	int psci_ret;
	u_register_t lead_mpid = read_mpidr_el1() & MPID_MASK;

	SKIP_TEST_IF_TSP_NOT_PRESENT();

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
				(uintptr_t)test_irq_preempted_std_smc_fn, 0);
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
	return test_irq_preempted_std_smc_fn();
}

/*
 * Test routine for non-lead CPUs for test_resume_preempted_std_smc_other_cpus.
 */
static test_result_t test_resume_preempted_std_smc_other_cpus_non_lead_fn(void)
{
	test_result_t result = TEST_RESULT_SUCCESS;

	u_register_t mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/*
	 * Try to resume the STD SMC invoked from the lead CPU. It shouldn't be
	 * able to do it.
	 */

	smc_args std_smc_args;
	std_smc_args.fid = TSP_FID_RESUME;
	smc_ret_values smc_ret = tftf_smc(&std_smc_args);
	if (smc_ret.ret0 != SMC_UNKNOWN) {
		tftf_testcase_printf(
			"SMC @ lead CPU returned 0x%llX 0x%llX 0x%llX instead of SMC_UNKNOWN\n",
			(unsigned long long)smc_ret.ret0,
			(unsigned long long)smc_ret.ret1,
			(unsigned long long)smc_ret.ret2);
		result = TEST_RESULT_FAIL;
	}

	/* Signal to the lead CPU that the calling CPU has finished the test */
	tftf_send_event(&cpu_has_finished_test[core_pos]);

	return result;
}

/*
 * @Test_Aim@ Multicore preemption test. For a MP Secure Payload, the
 * pre-emption on one CPU should not affect the other CPU. Trying to resume
 * one STD SMC that was preempted on one CPU shouldn't be possible from any
 * other CPU.
 *
 * Steps: 1. Issue Standard SMC and try preempting it via IRQ on lead CPU.
 *        2. Try to resume it from the rest of the CPUs sequentially.
 *        3. Resume the preempted SMC from the lead CPU and verify the result.
 *
 * Returns SUCCESS if step 2 fails and steps 1 and 3 succeed, else failure.
 */
test_result_t test_resume_preempted_std_smc_other_cpus(void)
{
	int i;
	u_register_t cpu_mpid;
	unsigned int cpu_node, core_pos;
	int psci_ret;

	u_register_t lead_mpid = read_mpidr_el1() & MPID_MASK;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/*
	 * Invoke a STD SMC that will be pre-empted.
	 */
	if (preempt_std_smc_on_this_cpu() != TEST_RESULT_SUCCESS) {
		return TEST_RESULT_FAIL;
	}

	/*
	 * Try to resume the STD SMC from the rest of CPUs. It shouldn't be
	 * possible.
	 */

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		tftf_init_event(&cpu_has_finished_test[i]);
	}

	/* Power on all CPUs and perform test sequentially. */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it's the one with the pre-empted STD SMC. */
		if (cpu_mpid == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(cpu_mpid);

		psci_ret = tftf_cpu_on(cpu_mpid,
			(uintptr_t)test_resume_preempted_std_smc_other_cpus_non_lead_fn, 0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU %d (rc = %d)\n",
				core_pos, psci_ret);
			return TEST_RESULT_FAIL;
		}

		/* Wait until the test is finished to begin with the next CPU. */
		tftf_wait_for_event(&cpu_has_finished_test[core_pos]);
	}

	/*
	 * Try to resume the STD SMC from the lead CPU. It should be able to do
	 * it and to return the correct result.
	 */
	return resume_std_smc_on_this_cpu();
}

/* Test routine for secondary CPU for test_resume_different_cpu_preempted_std_smc */
static test_result_t test_resume_different_cpu_preempted_std_smc_non_lead_fn(void)
{
	smc_args std_smc_args;
	smc_ret_values smc_ret;

	u_register_t mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Signal to the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_has_entered_test[core_pos]);

	/* Register and enable SGI. SGIs #0 - #6 are freely available. */
	if (register_and_enable_test_sgi_handler(core_pos) != 0) {
		/* Signal to the lead CPU that the calling CPU has finished */
		tftf_send_event(&cpu_has_finished_test[core_pos]);
		return TEST_RESULT_FAIL;
	}

	/* Set PSTATE.I to 0. */
	disable_irq();

	/*
	 * Send SGI to itself. It can't be handled because the interrupts are
	 * disabled.
	 */
	requested_irq_received[core_pos] = 0;

	tftf_send_sgi(IRQ_NS_SGI_0, core_pos);

	/*
	 * Invoke an STD SMC. Should be pre-empted because of the SGI that is
	 * waiting. It has to be different than the one invoked from the lead
	 * CPU.
	 */
	std_smc_args.fid = TSP_STD_FID(TSP_MUL);
	std_smc_args.arg1 = TEST_VALUE_1;
	std_smc_args.arg2 = TEST_VALUE_2;
	smc_ret = tftf_smc(&std_smc_args);
	if (smc_ret.ret0 != TSP_SMC_PREEMPTED) {
		tftf_testcase_printf(
			"SMC @ CPU %d returned 0x%llX instead of TSP_SMC_PREEMPTED.\n",
			core_pos, (unsigned long long)smc_ret.ret0);
		enable_irq();
		unregister_and_disable_test_sgi_handler();
		/* Signal to the lead CPU that the calling CPU has finished */
		tftf_send_event(&cpu_has_finished_test[core_pos]);
		return TEST_RESULT_FAIL;
	}

	/* Set PSTATE.I to 1. Let the SGI be handled. */
	enable_irq();

	/* Cleanup. Disable and unregister SGI handler. */
	unregister_and_disable_test_sgi_handler();

	/*
	 * Check that the SGI has been handled, but don't fail if it hasn't
	 * because there is no guarantee that it will have actually happened at
	 * this point.
	 */
	if (requested_irq_received[core_pos] == 0) {
		VERBOSE("SGI not handled @ CPU %d\n", core_pos);
	}

	/* Resume the STD SMC. Verify result. */
	std_smc_args.fid = TSP_FID_RESUME;
	smc_ret = tftf_smc(&std_smc_args);
	if ((smc_ret.ret0 != 0) || (smc_ret.ret1 != TEST_VALUE_1*TEST_VALUE_1)
	    || (smc_ret.ret2 != TEST_VALUE_2*TEST_VALUE_2)) {
		tftf_testcase_printf(
			"SMC @ CPU %d returned 0x%llX 0x%llX 0x%llX instead of 0x0 0x%X 0x%X\n",
			core_pos, (unsigned long long)smc_ret.ret0,
			(unsigned long long)smc_ret.ret1,
			(unsigned long long)smc_ret.ret2,
			TEST_VALUE_1*2, TEST_VALUE_2*2);
		/* Signal to the lead CPU that the calling CPU has finished */
		tftf_send_event(&cpu_has_finished_test[core_pos]);
		return TEST_RESULT_FAIL;
	}

	/* Try to resume the lead CPU STD SMC. Verify result. */
	std_smc_args.fid = TSP_FID_RESUME;
	smc_ret = tftf_smc(&std_smc_args);
	if (smc_ret.ret0 != SMC_UNKNOWN) {
		tftf_testcase_printf(
			"SMC @ CPU %d returned 0x%llX 0x%llX 0x%llX instead of SMC_UNKNOWN\n",
			core_pos, (unsigned long long)smc_ret.ret0,
			(unsigned long long)smc_ret.ret1,
			(unsigned long long)smc_ret.ret2);
		/* Signal to the lead CPU that the calling CPU has finished */
		tftf_send_event(&cpu_has_finished_test[core_pos]);
		return TEST_RESULT_FAIL;
	}

	/* Signal to the lead CPU that the calling CPU has finished the test */
	tftf_send_event(&cpu_has_finished_test[core_pos]);
	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Multicore preemption test. For a MP Secure Payload, the
 * pre-emption on one CPU should not affect the other CPU. Trying to resume
 * one STD SMC pre-empted on one CPU shouldn't be possible from any other CPU
 * involved in the test, and the STD SMC that is resumed from each CPU should
 * be the same one that was invoked from it.
 *
 * Steps: 1. Lead and secondary CPUs set different preempted STD SMCs.
 *        2. Resume the preempted SMC from secondary CPU. Verify the result.
 *        3. Try to resume again to check if it can resume the lead SMC.
 *        4. Resume the preempted SMC from lead CPU. Verify the result.
 *
 * Returns SUCCESS if steps 1, 2 and 4 succeed and step 3 fails, else failure.
 */
test_result_t test_resume_different_cpu_preempted_std_smc(void)
{
	smc_args std_smc_args;
	smc_ret_values smc_ret;
	u_register_t cpu_mpid;
	unsigned int core_pos;
	int psci_ret;

	u_register_t lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int lead_pos = platform_get_core_pos(lead_mpid);

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	/*
	 * Generate a SGI on the lead CPU that can't be handled because the
	 * interrupts are disabled.
	 */
	register_and_enable_test_sgi_handler(lead_mpid);
	disable_irq();

	requested_irq_received[lead_pos] = 0;

	tftf_send_sgi(IRQ_NS_SGI_0, lead_pos);

	/*
	 * Invoke an STD SMC. Should be pre-empted because of the SGI that is
	 * waiting.
	 */
	std_smc_args.fid = TSP_STD_FID(TSP_ADD);
	std_smc_args.arg1 = TEST_VALUE_1;
	std_smc_args.arg2 = TEST_VALUE_2;
	smc_ret = tftf_smc(&std_smc_args);
	if (smc_ret.ret0 != TSP_SMC_PREEMPTED) {
		tftf_testcase_printf(
			"SMC @ lead CPU returned 0x%llX instead of TSP_SMC_PREEMPTED.\n",
			(unsigned long long)smc_ret.ret0);
		enable_irq();
		unregister_and_disable_test_sgi_handler();
		return TEST_RESULT_FAIL;
	}

	/* Set PSTATE.I to 1. Let the SGI be handled. */
	enable_irq();

	/* Cleanup. Disable and unregister SGI handler. */
	unregister_and_disable_test_sgi_handler();

	/*
	 * Check that the SGI has been handled, but don't fail if it hasn't
	 * because there is no guarantee that it will have actually happened at
	 * this point.
	 */
	if (requested_irq_received[lead_pos] == 0) {
		VERBOSE("SGI not handled @ lead CPU.\n");
	}

	/* Generate a preempted SMC in a secondary CPU. */
	cpu_mpid = tftf_find_any_cpu_other_than(lead_mpid);
	if (cpu_mpid == INVALID_MPID) {
		tftf_testcase_printf("Couldn't find another CPU.\n");
		return TEST_RESULT_FAIL;
	}

	core_pos = platform_get_core_pos(cpu_mpid);
	tftf_init_event(&cpu_has_finished_test[core_pos]);

	psci_ret = tftf_cpu_on(cpu_mpid, (uintptr_t)
			       test_resume_different_cpu_preempted_std_smc_non_lead_fn, 0);
	if (psci_ret != PSCI_E_SUCCESS) {
		tftf_testcase_printf("Failed to power on CPU %d (rc = %d)\n",
				     core_pos, psci_ret);
		return TEST_RESULT_FAIL;
	}

	/* Wait until the test is finished to continue. */
	tftf_wait_for_event(&cpu_has_finished_test[core_pos]);

	/*
	 * Try to resume the STD SMC from the lead CPU. It should be able resume
	 * the one it generated before and to return the correct result.
	 */
	std_smc_args.fid = TSP_FID_RESUME;
	smc_ret = tftf_smc(&std_smc_args);
	if ((smc_ret.ret0 != 0) || (smc_ret.ret1 != TEST_VALUE_1 * 2) ||
	    (smc_ret.ret2 != TEST_VALUE_2 * 2)) {
		tftf_testcase_printf(
			"SMC @ lead CPU returned 0x%llX 0x%llX 0x%llX instead of 0x0 0x%X 0x%X\n",
			(unsigned long long)smc_ret.ret0,
			(unsigned long long)smc_ret.ret1,
			(unsigned long long)smc_ret.ret2,
			TEST_VALUE_1*2, TEST_VALUE_2*2);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*******************************************************************************
 * Test PSCI APIs while preempted.
 ******************************************************************************/

/*
 * First part of the test routine for test_psci_cpu_on_off_preempted.
 * Prepare a pre-empted STD SMC.
 */
static test_result_t test_psci_cpu_on_off_preempted_non_lead_fn_1(void)
{
	test_result_t result = TEST_RESULT_SUCCESS;

	u_register_t mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	if (preempt_std_smc_on_this_cpu() != TEST_RESULT_SUCCESS) {
		return TEST_RESULT_FAIL;
	}

	/*
	 * Signal to the lead CPU that the calling CPU has entered the test
	 * conditions for the second part.
	 */
	tftf_send_event(&cpu_has_entered_test[core_pos]);

	/*
	 * Now this CPU has to be turned off. Since this is not a lead CPU, it
	 * will be done in run_tests(). If it was done here, cpus_cnt wouldn't
	 * decrement and the tftf would think there is still a CPU running, so
	 * it wouldn't finish.
	 *
	 * The result will be overwritten when the second part of the test is
	 * executed.
	 */
	return result;
}

/*
 * Second part of the test routine for test_psci_cpu_on_off_preempted.
 * Try to resume the previously pre-empted STD SMC.
 */
static test_result_t test_psci_cpu_on_off_preempted_non_lead_fn_2(void)
{
	test_result_t result;

	u_register_t mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Try to resume the STD SMC. Check that it fails. */
	result = resume_fail_std_smc_on_this_cpu();

	/* Signal to the lead CPU that the calling CPU has finished the test */
	tftf_send_event(&cpu_has_finished_test[core_pos]);

	return result;
}

/*
 * @Test_Aim@  Resume preempted STD SMC after PSCI CPU OFF/ON cycle.
 *
 * Steps: 1. Each CPU sets a preempted STD SMC.
 *        2. They send an event to the lead CPU and call PSCI CPU OFF.
 *        3. The lead CPU invokes PSCI CPU ON for the secondaries (warm boot).
 *        4. Try to resume the preempted STD SMC on secondary CPUs.
 *
 * Returns SUCCESS if steps 1, 2 or 3 succeed and step 4 fails, else failure.
 */
test_result_t test_psci_cpu_on_off_preempted_std_smc(void)
{
	int i;
	int all_powered_down;
	u_register_t cpu_mpid;
	unsigned int cpu_node, core_pos;
	int psci_ret;
	u_register_t lead_mpid = read_mpidr_el1() & MPID_MASK;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		tftf_init_event(&cpu_has_entered_test[i]);
		tftf_init_event(&cpu_has_finished_test[i]);
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
				(uintptr_t)test_psci_cpu_on_off_preempted_non_lead_fn_1, 0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("Failed to power on CPU %d (rc = %d)\n",
				core_pos, psci_ret);
			return TEST_RESULT_FAIL;
		}
	}

	/* Wait for non-lead CPUs to exit the first part of the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_has_entered_test[core_pos]);
	}

	/* Check that all secondary CPUs are powered off. */
	all_powered_down = 0;
	while (all_powered_down == 0) {
		all_powered_down = 1;
		for_each_cpu(cpu_node) {
			cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
			if (cpu_mpid == lead_mpid) {
				continue;
			}
			if (tftf_is_cpu_online(cpu_mpid) != 0) {
				all_powered_down = 0;
			}
		}
	}

	/* Start the second part of the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already powered on */
		if (cpu_mpid == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(cpu_mpid);

		psci_ret = tftf_cpu_on(cpu_mpid,
				(uintptr_t)test_psci_cpu_on_off_preempted_non_lead_fn_2, 0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf("Failed to power on CPU 0x%x (rc = %d)\n",
				core_pos, psci_ret);
			return TEST_RESULT_FAIL;
		}
	}

	/* Wait for non-lead CPUs to finish the second part of the test. */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid) {
			continue;
		}

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_has_finished_test[core_pos]);
	}

	return TEST_RESULT_SUCCESS;
}

/******************************************************************************/

/*
 * @Test_Aim@  Resume preempted STD SMC after PSCI SYSTEM SUSPEND (in case it is
 * supported).
 *
 * Steps: 1. The lead CPU sets a preempted STD SMC.
 *        2. It calls PSCI SYSTEM SUSPEND with a wakeup timer for 1 sec.
 *        3. Try to resume the preempted STD SMC.
 *
 * Returns SUCCESS if steps 1 and 2 succeed and step 3 fails.
 */
test_result_t test_psci_system_suspend_preempted_std_smc(void)
{
	int psci_ret;
	int result = TEST_RESULT_SUCCESS;

	u_register_t lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int lead_pos = platform_get_core_pos(lead_mpid);

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	if (!is_psci_sys_susp_supported()) {
		tftf_testcase_printf(
			"SYSTEM_SUSPEND is not supported.\n");
		return TEST_RESULT_SKIPPED;
	}

	if (preempt_std_smc_on_this_cpu() != TEST_RESULT_SUCCESS) {
		return TEST_RESULT_FAIL;
	}

	if (!is_sys_suspend_state_ready()) {
		result = TEST_RESULT_FAIL;
	}

	/* Prepare wakeup timer. IRQs need to be enabled. */
	wakeup_irq_received[lead_pos] = 0;

	tftf_timer_register_handler(suspend_wakeup_handler);

	/* Program timer to fire interrupt after timer expires */
	tftf_program_timer(SUSPEND_TIME_1_SEC);

	/* Issue PSCI_SYSTEM_SUSPEND. */
	psci_ret = tftf_system_suspend();

	while (!wakeup_irq_received[lead_pos])
		;

	if (psci_ret != PSCI_E_SUCCESS) {
		mp_printf("SYSTEM_SUSPEND from lead CPU failed. ret: 0x%x\n",
			  psci_ret);
		result = TEST_RESULT_FAIL;
	}

	/* Remove timer after waking up.*/
	tftf_cancel_timer();
	tftf_timer_unregister_handler();

	if (resume_fail_std_smc_on_this_cpu() != TEST_RESULT_SUCCESS) {
		result = TEST_RESULT_FAIL;
	}

	return result;
}
