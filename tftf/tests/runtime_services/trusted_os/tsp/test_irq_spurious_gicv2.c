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
#include <test_helpers.h>
#include <tftf_lib.h>

#define TEST_VALUE_1 4
#define TEST_VALUE_2 6

#define TEST_SPURIOUS_ITERATIONS_COUNT	1000000

#define TEST_SPI_ID	(MIN_SPI_ID + 2)

static event_t cpu_ready[PLATFORM_CORE_COUNT];
static volatile int requested_irq_received[PLATFORM_CORE_COUNT];
static volatile int test_finished_flag;

static volatile int spurious_count[PLATFORM_CORE_COUNT];
static volatile int preempted_count[PLATFORM_CORE_COUNT];

/* Dummy handler that sets a flag so as to check it has been called. */
static int test_handler(void *data)
{
	u_register_t core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);

	assert(requested_irq_received[core_pos] == 0);

	requested_irq_received[core_pos] = 1;

	return 0;
}

/* Dummy handler that increases a variable to check if it has been called. */
static int test_spurious_handler(void *data)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());

	spurious_count[core_pos]++;

	return 0;
}

/* Helper function for test_multicore_spurious_interrupt. */
static test_result_t test_multicore_spurious_interrupt_non_lead_fn(void)
{
	test_result_t result = TEST_RESULT_SUCCESS;
	u_register_t mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Signal to the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&cpu_ready[core_pos]);

	while (test_finished_flag == 0) {

		smc_args std_smc_args;
		smc_ret_values smc_ret;

		/* Invoke an STD SMC. */
		std_smc_args.fid = TSP_STD_FID(TSP_ADD);
		std_smc_args.arg1 = TEST_VALUE_1;
		std_smc_args.arg2 = TEST_VALUE_2;
		smc_ret = tftf_smc(&std_smc_args);

		while (result != TEST_RESULT_FAIL) {
			if (smc_ret.ret0 == 0) {
				/* Verify result */
				if ((smc_ret.ret1 != TEST_VALUE_1 * 2) ||
				    (smc_ret.ret2 != TEST_VALUE_2 * 2)) {
					tftf_testcase_printf(
						"SMC @ CPU %d returned 0x0 0x%llX 0x%llX instead of 0x0 0x%X 0x%X\n",
						core_pos,
						(unsigned long long)smc_ret.ret1,
						(unsigned long long)smc_ret.ret2,
						TEST_VALUE_1 * 2, TEST_VALUE_2 * 2);
					result = TEST_RESULT_FAIL;
				} else {
					/* Correct, exit inner loop */
					break;
				}
			} else if (smc_ret.ret0 == TSP_SMC_PREEMPTED) {
				/* Resume the STD SMC. */
				std_smc_args.fid = TSP_FID_RESUME;
				smc_ret = tftf_smc(&std_smc_args);
				preempted_count[core_pos]++;
			} else {
				/* Error */
				tftf_testcase_printf(
					"SMC @ lead CPU returned 0x%llX 0x%llX 0x%llX\n",
					(unsigned long long)smc_ret.ret0,
					(unsigned long long)smc_ret.ret1,
					(unsigned long long)smc_ret.ret2);
				mp_printf("Panic <others> %d\n", core_pos);
				result = TEST_RESULT_FAIL;
			}
		}

		if (result == TEST_RESULT_FAIL)
			break;
	}

	/* Signal to the lead CPU that the calling CPU has finished the test */
	tftf_send_event(&cpu_ready[core_pos]);

	return result;
}

/*
 * @Test_Aim@  Test Spurious interrupt handling. GICv2 only. Only works if TF
 * is compiled with TSP_NS_INTR_ASYNC_PREEMPT = 0.
 *
 * Steps: 1. Setup SPI handler and spurious interrupt handler on the lead CPU.
 *        2. Redirect SPI interrupts to all CPUs.
 *        3. Turn on secondary CPUs and make them invoke STD SMC all time.
 *        4. The lead CPU starts a loop that triggers a SPI so that all CPUs
 *           will try to handle it.
 *        5. The CPUs that can't handle the SPI will receive a spurious
 *           interrupt and increase a counter.
 *        6. Check that there have been spurious interrupts. Not necessarily
 *           the number of (CPU - 1) * iterations as the SMC may need time to
 *           handle.
 *
 * Returns SUCCESS if all steps succeed, else failure.
 */
test_result_t test_multicore_spurious_interrupt(void)
{
	int i, j;
	u_register_t cpu_mpid;
	unsigned int cpu_node, core_pos;
	int psci_ret;
	int ret;

	u_register_t lead_mpid = read_mpidr_el1() & MPID_MASK;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	if (is_gicv3_mode()) {
		tftf_testcase_printf("Detected GICv3. Need GICv2.\n");
		return TEST_RESULT_SKIPPED;
	}

	ret = tftf_irq_register_handler(GIC_SPURIOUS_INTERRUPT,
					     test_spurious_handler);
	if (ret != 0) {
		tftf_testcase_printf(
			"Failed to register spurious handler. Error = %d\n",
			ret);
		return TEST_RESULT_SKIPPED;
	}

	/* Reset test variables and boot secondary cores. */

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		spurious_count[i] = 0;
		preempted_count[i] = 0;
	}

	test_finished_flag = 0;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++)
		tftf_init_event(&cpu_ready[i]);

	dsbsy();

	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		psci_ret = tftf_cpu_on(cpu_mpid,
			(uintptr_t)test_multicore_spurious_interrupt_non_lead_fn, 0);
		if (psci_ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
				"Failed to power on CPU 0x%x (%d)\n",
				(unsigned int)cpu_mpid, psci_ret);
			test_finished_flag = 1;
			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for non-lead CPUs to enter the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
		tftf_init_event(&cpu_ready[i]);
	}

	/* Wait until tftf_init_event is seen by all cores */
	dsbsy();

	/* Register SPI handler (for all CPUs) and enable it. */
	ret = tftf_irq_register_handler(TEST_SPI_ID, test_handler);
	if (ret != 0) {
		tftf_testcase_printf(
			"Failed to register SPI handler @ lead CPU. Error code = %d\n",
			ret);
		tftf_irq_unregister_handler(GIC_SPURIOUS_INTERRUPT);
		test_finished_flag = 1;
		return TEST_RESULT_SKIPPED;
	}

	/* Enable IRQ and route it to this CPU. */
	tftf_irq_enable(TEST_SPI_ID, GIC_HIGHEST_NS_PRIORITY);

	/* Route interrupts to all CPUs */
	gicv2_set_itargetsr_value(TEST_SPI_ID, 0xFF);

	for (j = 0; j < TEST_SPURIOUS_ITERATIONS_COUNT; j++) {

		/* Clear handled flags. */
		for (i = 0; i < PLATFORM_CORE_COUNT; i++)
			requested_irq_received[i] = 0;
		dsbsy();

		/* Request SPI */
		gicv2_gicd_set_ispendr(TEST_SPI_ID);

		/* Wait until it is handled. */
		int wait = 1;

		while (wait) {
			for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
				if (requested_irq_received[i])
					wait = 0;
			}
		}
	}

	test_finished_flag = 1;

	/* Wait for non-lead CPUs to finish the test */
	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (cpu_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(cpu_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	/* Cleanup. */
	tftf_irq_disable(TEST_SPI_ID);

	tftf_irq_unregister_handler(TEST_SPI_ID);
	tftf_irq_unregister_handler(GIC_SPURIOUS_INTERRUPT);

	/* Check results. */
	unsigned int total_spurious_count = 0;
	unsigned int total_preempted_count = 0;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		total_spurious_count += spurious_count[i];
		total_preempted_count += preempted_count[i];
	}

	/* Check that the test has tested the behaviour. */
	if (total_spurious_count == 0) {
		tftf_testcase_printf("No spurious interrupts were handled.\n"
			"The TF-A must be compiled with TSP_NS_INTR_ASYNC_PREEMPT = 0\n");
		/*
		 * Don't flag the test as failed in case the TF-A was compiled
		 * with TSP_NS_INTR_ASYNC_PREEMPT=1.
		 */
		return TEST_RESULT_SKIPPED;
	}


	if (total_preempted_count == 0) {
		tftf_testcase_printf("No preempted STD SMCs.\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
