/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <amu.h>
#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <irq.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <tftf_lib.h>
#include <timer.h>

#define SUSPEND_TIME_1_SEC	1000

static volatile int wakeup_irq_received[PLATFORM_CORE_COUNT];

/* Dummy timer handler that sets a flag to check it has been called. */
static int suspend_wakeup_handler(void *data)
{
	u_register_t core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);

	assert(wakeup_irq_received[core_pos] == 0);

	wakeup_irq_received[core_pos] = 1;

	return 0;
}

/*
 * Helper function to suspend a CPU to power level 0 and wake it up with
 * a timer.
 */
static test_result_t suspend_and_resume_this_cpu(void)
{
	uint32_t stateid;
	int psci_ret;
	test_result_t result = TEST_RESULT_SUCCESS;

	u_register_t core_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(core_mpid);

	/* Prepare wakeup timer. IRQs need to be enabled. */
	wakeup_irq_received[core_pos] = 0;

	tftf_timer_register_handler(suspend_wakeup_handler);

	/* Program timer to fire interrupt after timer expires */
	tftf_program_timer(SUSPEND_TIME_1_SEC);

	/*
	 * Suspend the calling CPU to power level 0 and power
	 * state.
	 */
	psci_ret = tftf_psci_make_composite_state_id(PSTATE_AFF_LVL_0,
						     PSTATE_TYPE_POWERDOWN,
						     &stateid);
	if (psci_ret != PSCI_E_SUCCESS) {
		mp_printf("Failed to make composite state ID @ CPU %d. rc = %x\n",
			  core_pos, psci_ret);
		result = TEST_RESULT_FAIL;
	} else {
		unsigned int power_state = tftf_make_psci_pstate(PSTATE_AFF_LVL_0,
						PSTATE_TYPE_POWERDOWN, stateid);
		psci_ret = tftf_cpu_suspend(power_state);

		if (!wakeup_irq_received[core_pos]) {
			mp_printf("Didn't receive wakeup IRQ in CPU %d.\n",
				  core_pos);
			result = TEST_RESULT_FAIL;
		}

		if (psci_ret != PSCI_E_SUCCESS) {
			mp_printf("Failed to suspend from CPU %d. ret: %x\n",
				  core_pos, psci_ret);
			result = TEST_RESULT_FAIL;
		}
	}

	/* Wake up. Remove timer after waking up.*/
	tftf_cancel_timer();
	tftf_timer_unregister_handler();

	return result;
}

/*
 * Helper function that checks whether the value of a group0 counter is valid
 * or not. The first 3 counters (0,1,2) cannot have values of zero but the last
 * counter that counts "memory stall cycles" can have a value of zero, under
 * certain circumstances.
 *
 * Return values:
 *    0 = valid counter value
 *   -1 = invalid counter value
 */
static int amu_group0_cnt_valid(unsigned int idx, uint64_t value)
{
	int answer = 0;

	if ((idx <= 2) && (value == 0)) {
		answer = -1;
	}

	return answer;
}

/*
 * Check that group0 counters are valid. As EL3 has enabled the counters before
 * the first entry to NS world, the counters should have increased by the time
 * we reach this test case.
 */
test_result_t test_amu_valid_ctr(void)
{
	unsigned int i;

	if (amu_get_version() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	/* If counters are not enabled, then skip the test */
	if (read_amcntenset0_el0() != AMU_GROUP0_COUNTERS_MASK) {
		return TEST_RESULT_SKIPPED;
	}

	for (i = 0U; i < AMU_GROUP0_NR_COUNTERS; i++) {
		uint64_t value;

		value = amu_group0_cnt_read(i);
		if (amu_group0_cnt_valid(i, value)) {
			tftf_testcase_printf("Group0 counter %d has invalid value %lld\n", i, value);
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Check that the counters are non-decreasing during
 * a suspend/resume cycle.
 */
test_result_t test_amu_suspend_resume(void)
{
	uint64_t group0_ctrs[AMU_GROUP0_NR_COUNTERS];
	unsigned int i;

	if (amu_get_version() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	/* If counters are not enabled, then skip the test */
	if (read_amcntenset0_el0() != AMU_GROUP0_COUNTERS_MASK)
		return TEST_RESULT_SKIPPED;

	/* Save counters values before suspend */
	for (i = 0U; i < AMU_GROUP0_NR_COUNTERS; i++) {
		group0_ctrs[i] = amu_group0_cnt_read(i);
	}

	/*
	 * If FEAT_AMUv1p1 supported then make sure the save/restore works for
	 * virtual counter values.  Write known values into the virtual offsets
	 * and then make sure they are still there after resume.  The virtual
	 * offset registers are only accessible in AARCH64 mode in EL2 or EL3.
	 */
#if __aarch64__
	if (amu_get_version() >= ID_AA64PFR0_AMU_V1P1) {
		/* Enabling voffsets in HCR_EL2. */
		write_hcr_el2(read_hcr_el2() | HCR_AMVOFFEN_BIT);

		/* Writing known values into voffset registers. */
		amu_group0_voffset_write(0U, 0xDEADBEEF);
		amu_group0_voffset_write(2U, 0xDEADBEEF);
		amu_group0_voffset_write(3U, 0xDEADBEEF);

#if AMU_GROUP1_NR_COUNTERS
		u_register_t amcg1idr = read_amcg1idr_el0() >> 16;

		for (i = 0U; i < AMU_GROUP1_NR_COUNTERS; i++) {
			if (((amcg1idr >> i) & 1U) != 0U) {
				amu_group1_voffset_write(i, 0xDEADBEEF);
			}
		}
#endif
	}
#endif

	/* Suspend/resume current core */
	suspend_and_resume_this_cpu();

	/*
	 * Check if counter values are >= than the stored values.
	 * If they are not, the AMU context save/restore in EL3 is buggy.
	 */
	for (i = 0; i < AMU_GROUP0_NR_COUNTERS; i++) {
		uint64_t value;

		value = amu_group0_cnt_read(i);
		if (value < group0_ctrs[i]) {
			tftf_testcase_printf("Invalid counter value: before: %llx, after: %llx\n",
				(unsigned long long)group0_ctrs[i],
				(unsigned long long)value);
			return TEST_RESULT_FAIL;
		}
	}

#if __aarch64__
	if (amu_get_version() >= ID_AA64PFR0_AMU_V1P1) {
		for (i = 0U; i < AMU_GROUP0_NR_COUNTERS; i++) {
			if ((i != 1U) &&
				(amu_group0_voffset_read(i) != 0xDEADBEEF)) {
				tftf_testcase_printf(
					"Invalid G0 voffset %u: 0x%llx\n", i,
					amu_group0_voffset_read(i));
				return TEST_RESULT_FAIL;
			}
		}

#if AMU_GROUP1_NR_COUNTERS
		u_register_t amcg1idr = read_amcg1idr_el0() >> 16;

		for (i = 0U; i < AMU_GROUP1_NR_COUNTERS; i++) {
			if (((amcg1idr >> i) & 1U) != 0U) {
				if (amu_group1_voffset_read(i) != 0xDEADBEEF) {
					tftf_testcase_printf("Invalid G1 " \
						"voffset %u: 0x%llx\n", i,
						amu_group1_voffset_read(i));
					return TEST_RESULT_FAIL;
				}
			}
		}
#endif
	}
#endif

	return TEST_RESULT_SUCCESS;
}
