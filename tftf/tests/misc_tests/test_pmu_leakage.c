/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains tests that try to leak information from the secure world
 * to the non-secure world (EL2) by using the PMU counters.
 *
 * The tests assume that the PMU (PMUv3) is implemented on the target, since
 * TF-A performs initialization of the PMU and guards against PMU counter
 * leakage.
 *
 * The non-secure world can use system registers to configure the PMU such that
 * it increments counters in the secure world. Depending on the implemented
 * features, the secure world can prohibit counting via the following:
 *	-v8.2 Debug not implemented:
 *	    |-- Prohibit general event counters and the cycle counter:
 *		MDCR_EL3.SPME == 0 && !ExternalSecureNoninvasiveDebugEnabled()
 *		Since ExternalSecureNoninvasiveDebugEnabled() is a hardware
 *		line, it is not available on FVP and will therefore cause the
 *		tests to fail.
 *		The only other way is to disable the PMCR_EL0.E bit. This will
 *		disable counting altogether, but since this fix is not desired
 *		in TF-A, the tests have to be skipped if v8.2 Debug is not
 *		implemented.
 *
 *	-v8.2 Debug implemented:
 *	    |-- Prohibit general event counters: MDCR_EL3.SPME == 0. This bit
 *		resets to 0, so by default general events should not be counted
 *		in the secure world.
 *	    |-- Prohibit cycle counter: MDCR_EL3.SPME == 0 && PMCR_EL0.DP == 1.
 *		This counter is only affected by MDCR_EL3.SPME if the
 *		PMCR_EL0.DP bit is set.
 *
 *	-v8.5 implemented:
 *	    |-- Prohibit general event counters: as in v8.2 Debug.
 *	    |-- Prohibit cycle counter: MDCR_EL3.SCCD == 1
 */

#include <drivers/arm/arm_gic.h>
#include <irq.h>
#include <platform.h>
#include <power_management.h>
#include <sgi.h>
#include <string.h>
#include <test_helpers.h>

#ifdef AARCH64
#define ITERATIONS_CNT 1000

/*
 * A maximum of +10% deviation in event counts is tolerated.
 * This is useful for testing on real hardware where event counts are usually
 * not the same between runs. The large iteration count should cause the
 * average event count to converge to values very close to baseline when the
 * secure world successfully prohibits PMU counters from incrementing.
 */
#define ALLOWED_DEVIATION 10

/*
 * An invalid SMC function number.
 * Used to establish a base value for PMU counters on each test.
 */
#define INVALID_FN 0x666

struct pmu_event_info {
	unsigned long long min;
	unsigned long long max;
	unsigned long long avg;
};

static inline void configure_pmu_cntr0(const uint32_t event)
{
	/*
	 * Disabling the P bit tells the counter to increment at EL1.
	 * Setting the NSK bit to be different from the P bit further tells the
	 * counter NOT to increment at non-secure EL1. Combined with the P bit,
	 * the effect is to tell the counter to increment at secure EL1.
	 * Setting the M bit to be equal to the P bit tells the counter to
	 * increment at EL3.
	 * Disabling the NSH bit tells the counter NOT to increment at
	 * non-secure EL2.
	 * Setting the SH bit to be different to the NSH bit tells the counter
	 * to increment at secure EL2.
	 * The counter therefore is told to count only at secure EL1, secure EL2
	 * and EL3. This is to ensure maximum accuracy of the results, since we
	 * are only interested if the secure world is leaking PMU counters.
	 */
	write_pmevtyper0_el0(
		(read_pmevtyper0_el0() | PMEVTYPER_EL0_NSK_BIT |
		 PMEVTYPER_EL0_SH_BIT) &
		~(PMEVTYPER_EL0_P_BIT | PMEVTYPER_EL0_NSH_BIT |
		  PMEVTYPER_EL0_M_BIT));

	/*
	 * Write to the EVTCOUNT bits to tell the counter which event to
	 * monitor.
	 */
	write_pmevtyper0_el0(
		(read_pmevtyper0_el0() & ~PMEVTYPER_EL0_EVTCOUNT_BITS) | event);

	/* Setting the P[n] bit enables counter n */
	write_pmcntenset_el0(
		read_pmcntenset_el0() | PMCNTENSET_EL0_P_BIT(0));
}

static inline void configure_pmu_cycle_cntr(void)
{
	/*
	 * Disabling the P bit tells the counter to increment at EL1.
	 * Setting the NSK bit to be different from the P bit further tells the
	 * counter NOT to increment at non-secure EL1. Combined with the P bit,
	 * the effect is to tell the counter to increment at secure EL1.
	 * Setting the M bit to be equal to the P bit tells the counter to
	 * increment at EL3.
	 * Disabling the NSH bit tells the counter NOT to increment at
	 * non-secure EL2.
	 * Setting the SH bit to be different to the NSH bit tells the counter
	 * to increment at secure EL2.
	 * The counter therefore is told to count only at secure EL1, secure EL2
	 * and EL3. This is to ensure maximum accuracy of the results, since we
	 * are only interested if the secure world is leaking PMU counters.
	 */
	write_pmccfiltr_el0(
		(read_pmccfiltr_el0() | PMCCFILTR_EL0_NSK_BIT |
		 PMCCFILTR_EL0_SH_BIT) &
		~(PMCCFILTR_EL0_P_BIT | PMCCFILTR_EL0_NSH_BIT |
		  PMCCFILTR_EL0_M_BIT));

	/* Setting the C bit enables the cycle counter in the PMU */
	write_pmcntenset_el0(
		read_pmcntenset_el0() | PMCNTENSET_EL0_C_BIT);

	/*
	 * Disabling the DP bit makes the cycle counter increment where
	 * prohibited by MDCR_EL3.SPME. If higher execution levels don't save
	 * and restore PMCR_EL0, then PMU information will be leaked.
	 */
	write_pmcr_el0(read_pmcr_el0() & ~PMCR_EL0_DP_BIT);
}

static inline void pmu_enable_counting(void)
{
	/*
	 * Setting the E bit gives [fine-grained] control to the PMCNTENSET_EL0
	 * register, which controls which counters can increment.
	 */
	write_pmcr_el0(read_pmcr_el0() | PMCR_EL0_E_BIT);
}

static unsigned long long profile_invalid_smc(u_register_t (*read_cntr_f)(void))
{
	unsigned long long evt_cnt;
	smc_args args = { INVALID_FN };

	evt_cnt = (*read_cntr_f)();
	tftf_smc(&args);
	evt_cnt = (*read_cntr_f)() - evt_cnt;

	return evt_cnt;
}

static unsigned long long profile_cpu_suspend(u_register_t (*read_cntr_f)(void))
{
	unsigned long long evt_cnt;
	unsigned int power_state;
	unsigned int stateid;

	tftf_psci_make_composite_state_id(MPIDR_AFFLVL0,
					  PSTATE_TYPE_STANDBY, &stateid);
	power_state = tftf_make_psci_pstate(MPIDR_AFFLVL0,
					    PSTATE_TYPE_STANDBY, stateid);

	tftf_irq_enable(IRQ_NS_SGI_0, GIC_HIGHEST_NS_PRIORITY);

	/*
	 * Mask IRQ to prevent the interrupt handler being invoked
	 * and clearing the interrupt. A pending interrupt will cause this
	 * CPU to wake-up from suspend.
	 */
	disable_irq();

	/* Configure an SGI to wake-up from suspend  */
	tftf_send_sgi(IRQ_NS_SGI_0,
		platform_get_core_pos(read_mpidr_el1() & MPID_MASK));

	evt_cnt = (*read_cntr_f)();
	tftf_cpu_suspend(power_state);
	evt_cnt = (*read_cntr_f)() - evt_cnt;

	/* Unmask the IRQ to let the interrupt handler to execute */
	enable_irq();
	isb();

	tftf_irq_disable(IRQ_NS_SGI_0);

	return evt_cnt;
}

static unsigned long long profile_fast_smc_add(u_register_t (*read_cntr_f)(void))
{
	unsigned long long evt_cnt;
	smc_args args = { TSP_FAST_FID(TSP_ADD), 4, 6 };

	evt_cnt = (*read_cntr_f)();
	tftf_smc(&args);
	evt_cnt = (*read_cntr_f)() - evt_cnt;

	return evt_cnt;
}

static void measure_event(u_register_t (*read_cntr_func)(void),
			  unsigned long long (*profile_func)(u_register_t (*read_cntr_f)(void)),
			  struct pmu_event_info *info)
{
	unsigned long long evt_cnt;
	unsigned long long min_cnt;
	unsigned long long max_cnt;
	unsigned long long avg_cnt;
	unsigned long long cnt_sum = 0;

	min_cnt = UINT64_MAX;
	max_cnt = 0;

	for (unsigned int i = 0; i < ITERATIONS_CNT; ++i) {
		evt_cnt = (*profile_func)(read_cntr_func);

		min_cnt = MIN(min_cnt, evt_cnt);
		max_cnt = MAX(max_cnt, evt_cnt);

		cnt_sum += evt_cnt;

		tftf_irq_disable(IRQ_NS_SGI_0);
	}

	avg_cnt = cnt_sum / ITERATIONS_CNT;

	info->avg = avg_cnt;
	info->min = min_cnt;
	info->max = max_cnt;

	tftf_testcase_printf(
		"Average count: %llu (ranging from %llu to %llu)\n",
		avg_cnt,
		min_cnt,
		max_cnt);
}

/*
 * Measure the number of retired writes to the PC in the PSCI_SUSPEND SMC.
 * This test only succeeds if no useful information about the PMU counters has
 * been leaked.
 */
test_result_t smc_psci_suspend_pc_write_retired(void)
{
	struct pmu_event_info baseline, cpu_suspend;

	SKIP_TEST_IF_ARCH_DEBUG_VERSION_LESS_THAN(
		ID_AA64DFR0_V8_2_DEBUG_ARCH_SUPPORTED);

	configure_pmu_cntr0(PMU_EV_PC_WRITE_RETIRED);
	pmu_enable_counting();

	tftf_testcase_printf("Getting baseline event count:\n");
	measure_event(read_pmevcntr0_el0, profile_invalid_smc, &baseline);
	tftf_testcase_printf("Profiling PSCI_SUSPEND_PC:\n");
	measure_event(read_pmevcntr0_el0, profile_cpu_suspend, &cpu_suspend);

	if (cpu_suspend.avg - baseline.avg > baseline.avg / ALLOWED_DEVIATION)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
}

/*
 * Measure the CPU cycles count of the PSCI_SUSPEND SMC.
 * This test only succeeds if no useful information about the PMU counters has
 * been leaked.
 */
test_result_t smc_psci_suspend_cycles(void)
{
	struct pmu_event_info baseline, cpu_suspend;

	SKIP_TEST_IF_ARCH_DEBUG_VERSION_LESS_THAN(
		ID_AA64DFR0_V8_2_DEBUG_ARCH_SUPPORTED);

	configure_pmu_cycle_cntr();
	pmu_enable_counting();

	tftf_testcase_printf("Getting baseline event count:\n");
	measure_event(read_pmccntr_el0, profile_invalid_smc, &baseline);
	tftf_testcase_printf("Profiling PSCI_SUSPEND_PC:\n");
	measure_event(read_pmccntr_el0, profile_cpu_suspend, &cpu_suspend);

	if (cpu_suspend.avg - baseline.avg > baseline.avg / ALLOWED_DEVIATION)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
}

/*
 * Measure the number of retired writes to the PC in the fast add SMC.
 * This test only succeeds if no useful information about the PMU counters has
 * been leaked.
 */
test_result_t fast_smc_add_pc_write_retired(void)
{
	struct pmu_event_info baseline, fast_smc_add;

	SKIP_TEST_IF_ARCH_DEBUG_VERSION_LESS_THAN(
		ID_AA64DFR0_V8_2_DEBUG_ARCH_SUPPORTED);

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	configure_pmu_cntr0(PMU_EV_PC_WRITE_RETIRED);
	pmu_enable_counting();

	tftf_testcase_printf("Getting baseline event count:\n");
	measure_event(read_pmevcntr0_el0, profile_invalid_smc, &baseline);
	tftf_testcase_printf("Profiling Fast Add SMC:\n");
	measure_event(read_pmevcntr0_el0, profile_fast_smc_add, &fast_smc_add);

	if (fast_smc_add.avg - baseline.avg > baseline.avg / ALLOWED_DEVIATION)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
}

/*
 * Measure the CPU cycles count of the fast add SMC.
 * This test only succeeds if no useful information about the PMU counters has
 * been leaked.
 */
test_result_t fast_smc_add_cycles(void)
{
	struct pmu_event_info baseline, fast_smc_add;

	SKIP_TEST_IF_ARCH_DEBUG_VERSION_LESS_THAN(
		ID_AA64DFR0_V8_2_DEBUG_ARCH_SUPPORTED);

	SKIP_TEST_IF_TSP_NOT_PRESENT();

	configure_pmu_cycle_cntr();
	pmu_enable_counting();

	tftf_testcase_printf("Getting baseline event count:\n");
	measure_event(read_pmccntr_el0, profile_invalid_smc, &baseline);
	tftf_testcase_printf("Profiling Fast Add SMC:\n");
	measure_event(read_pmccntr_el0, profile_fast_smc_add, &fast_smc_add);

	if (fast_smc_add.avg - baseline.avg > baseline.avg / ALLOWED_DEVIATION)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
}
#else
test_result_t smc_psci_suspend_pc_write_retired(void)
{
	INFO("%s skipped on AArch32\n", __func__);
	return TEST_RESULT_SKIPPED;
}

test_result_t smc_psci_suspend_cycles(void)
{
	INFO("%s skipped on AArch32\n", __func__);
	return TEST_RESULT_SKIPPED;
}

test_result_t fast_smc_add_pc_write_retired(void)
{
	INFO("%s skipped on AArch32\n", __func__);
	return TEST_RESULT_SKIPPED;
}

test_result_t fast_smc_add_cycles(void)
{
	INFO("%s skipped on AArch32\n", __func__);
	return TEST_RESULT_SKIPPED;
}
#endif
