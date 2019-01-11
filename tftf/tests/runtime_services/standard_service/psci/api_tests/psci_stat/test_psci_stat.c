/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <cassert.h>
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <events.h>
#include <irq.h>
#include <math_utils.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <spinlock.h>
#include <string.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <timer.h>

typedef struct psci_stat_data {
	u_register_t count;
	u_register_t residency;
} psci_stat_data_t;

/* Assuming 3 power levels as maximum */
#define MAX_STAT_STATES (PLAT_MAX_PWR_STATES_PER_LVL *	\
			PLAT_MAX_PWR_STATES_PER_LVL *	\
			PLAT_MAX_PWR_STATES_PER_LVL)

CASSERT(PLAT_MAX_PWR_LEVEL <= 2, assert_maximum_defined_stat_array_size_exceeded);

/*
 * The data structure holding stat information as queried by each CPU.
 * We don't worry about cache line thrashing.
 */
static psci_stat_data_t stat_data[PLATFORM_CORE_COUNT][PLAT_MAX_PWR_LEVEL + 1]
						       [MAX_STAT_STATES];

/*
 * Synchronization event for stat tests. A 2-D event array is used to
 * signal every CPU by each CPU. This caters for the fact some
 * CPUs may have higher performance than others and will not
 * cause the synchronization to fail.
 */
static event_t stat_sync[PLATFORM_CORE_COUNT][PLATFORM_CORE_COUNT];

/* Global variables to synchronize participating CPUs on wake-up */
static spinlock_t cpu_count_lock;
static volatile int cpu_count;
static volatile int participating_cpu_count;

/* Helper function to wait for CPUs participating in the test */
static void wait_for_participating_cpus(void)
{
	assert(participating_cpu_count <= PLATFORM_CORE_COUNT);

	spin_lock(&cpu_count_lock);
	cpu_count++;
	spin_unlock(&cpu_count_lock);

	assert(cpu_count <= PLATFORM_CORE_COUNT);

	while (cpu_count != participating_cpu_count)
		;
}

/* Helper function to detect support for PSCI STAT APIs in firmware */
static int is_psci_stat_supported(void)
{
	int ret_stat_count, ret_stat_res;

	ret_stat_count = tftf_get_psci_feature_info(SMC_PSCI_STAT_COUNT64);
	ret_stat_res = tftf_get_psci_feature_info(SMC_PSCI_STAT_RESIDENCY64);

	if (ret_stat_count == PSCI_E_NOT_SUPPORTED ||
			ret_stat_res == PSCI_E_NOT_SUPPORTED) {
		tftf_testcase_printf("PSCI STAT APIs are not supported"
				" in EL3 firmware\n");
		return 0;
	}

	return 1;
}

/*
 * Utility function to get the `stat` index into the `psci_stat_data_t` array
 * (which is the index into the 3rd dimension in the array).
 */
static int get_stat_idx(unsigned int pstateid_idx[], unsigned int lvl)
{
	int  i, stat_idx;
	/* Calculate the stat_idx */
	for (stat_idx = 0, i = lvl; i >= 0; i--) {
		assert((pstateid_idx[i] != PWR_STATE_INIT_INDEX) &&
			(pstateid_idx[i] < PLAT_MAX_PWR_STATES_PER_LVL));
		stat_idx += (pstateid_idx[i] * pow(PLAT_MAX_PWR_STATES_PER_LVL, i));
	}

	assert(stat_idx >= 0 && stat_idx < MAX_STAT_STATES);
	return stat_idx;
}

/*
 * Return the pointer the `psci_stat_data_t` array corresponding to
 * cpu index, power level and `stat` index (which is computed from
 * the pstateid_idx[]).
 */
static psci_stat_data_t *get_psci_stat_data(int cpu_idx,
		unsigned int pwrlvl,
		unsigned int pstateid_idx[])
{
	int stat_idx;

	/* Calculate the stat_idx */
	stat_idx = get_stat_idx(pstateid_idx, pwrlvl);
	return &stat_data[cpu_idx][pwrlvl][stat_idx];
}

/*
 * This function validates the current stat results against a previous
 * snapshot of stat information gathered in `stat_data` by
 * populate_all_stats_all_lvls() function. It does 2 kinds of validation:
 *
 * 1. Precise validation:
 * This ensures that the targeted power state as indicated by `pstateid_idx`
 * has incremented according to expectation. If it hasn't incremented,
 * then the targeted power state was downgraded by the platform (due to various
 * reasons) and in this case the queried stats should be equal to previous
 * stats.
 *
 * This validation is done for the targeted power level and all lower levels
 * for the given power state.
 *
 * 2. Imprecise validation:
 *
 * Iterate over all the power states and ensure that the previous stats for the
 * power state are never higher than the current one for power levels <=
 * targeted power level. For power states at higher power levels than targeted
 * power level, it should remain the same.
 */
static int validate_stat_result(unsigned int pstateid_idx[],
				unsigned int target_pwrlvl)
{
	unsigned long my_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int cpu_idx = platform_get_core_pos(my_mpid);
	unsigned int pwrlvl, susp_type, state_id, power_state;
	int ret;
	psci_stat_data_t current_stat_data;
	psci_stat_data_t *pstat_data;
	unsigned int local_pstateid_idx[PLAT_MAX_PWR_LEVEL + 1];

	assert(pstateid_idx[0] != PWR_STATE_INIT_INDEX);

	/* Create a local copy of pstateid_idx */
	memcpy(local_pstateid_idx, pstateid_idx, sizeof(local_pstateid_idx));

	/* First do the precise validation */
	do {
		/* Check if the power state is valid */
		ret = tftf_get_pstate_vars(&pwrlvl,
					&susp_type,
					&state_id,
					local_pstateid_idx);
		assert(ret == PSCI_E_SUCCESS);
		assert(pwrlvl <= PLAT_MAX_PWR_LEVEL);

		power_state = tftf_make_psci_pstate(pwrlvl,
				susp_type, state_id);

		/* Get current stat values for the power state */
		current_stat_data.residency = tftf_psci_stat_residency(my_mpid, power_state);
		current_stat_data.count = tftf_psci_stat_count(my_mpid, power_state);

		pstat_data = get_psci_stat_data(cpu_idx, pwrlvl, local_pstateid_idx);
		if ((pstat_data->residency == current_stat_data.residency) &&
				(pstat_data->count == current_stat_data.count)) {
			/*
			 * Targeted power state has been downgraded and the
			 * queried stats should be equal to  previous stats
			 */
			WARN("The power state 0x%x at pwrlvl %d has been"
					" downgraded by platform\n",
					power_state, pwrlvl);
		} else if ((pstat_data->residency > current_stat_data.residency) ||
				(pstat_data->count + 1 != current_stat_data.count)) {
			/*
			 * The previous residency is greater than current or the
			 * stat count has not incremented by 1 for the targeted
			 * power state. Return error in this case.
			 */
			ERROR("Precise validation failed. Stats for CPU %d at"
					" pwrlvl %d for power state 0x%x : Prev"
					" stats 0x%llx 0x%llx, current stats"
					" 0x%llx 0x%llx\n",
					cpu_idx, pwrlvl, power_state,
					(unsigned long long)pstat_data->residency,
					(unsigned long long)pstat_data->count,
					(unsigned long long)current_stat_data.residency,
					(unsigned long long)current_stat_data.count);
			return -1;
		} else {
			/*
			 * The stats are as expected for the targeted power state
			 * i.e previous residency <= current residency and
			 * previous stat count + 1 == current stat count.
			 */
			INFO("The difference in programmed time and residency"
				" time in us = %lld at power level %d\n",
				(unsigned long long)
				((current_stat_data.residency - pstat_data->residency)
				- (PLAT_SUSPEND_ENTRY_TIME * 1000)), pwrlvl);
		}

		local_pstateid_idx[pwrlvl] = PWR_STATE_INIT_INDEX;
	} while (pwrlvl);

	INIT_PWR_LEVEL_INDEX(local_pstateid_idx);

	/* Imprecise validation */
	do {
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, local_pstateid_idx);
		if (local_pstateid_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		/* Check if the power state is valid */
		ret = tftf_get_pstate_vars(&pwrlvl,
					&susp_type,
					&state_id,
					local_pstateid_idx);
		if (ret != PSCI_E_SUCCESS)
			continue;

		assert(pwrlvl <= PLAT_MAX_PWR_LEVEL);

		power_state = tftf_make_psci_pstate(pwrlvl,
				susp_type, state_id);

		pstat_data = get_psci_stat_data(cpu_idx, pwrlvl, local_pstateid_idx);

		current_stat_data.residency = tftf_psci_stat_residency(my_mpid,
								power_state);
		current_stat_data.count = tftf_psci_stat_count(my_mpid,
								power_state);
		if (pwrlvl <= target_pwrlvl) {
			/*
			 * For all power states that target power domain level
			 * <= `target_pwrlvl, the previous residency and count
			 * should never be greater than current.
			 */
			if ((pstat_data->residency > current_stat_data.residency) ||
					(pstat_data->count > current_stat_data.count)) {
				ERROR("Imprecise validation failed for"
					" pwrlvl <= target_pwrlvl. Stats for"
					" CPU %d for power state 0x%x. Prev"
					" stats 0x%llx 0x%llx, current stats 0x%llx"
					" 0x%llx\n",
					cpu_idx, power_state,
					(unsigned long long)pstat_data->residency,
					(unsigned long long)pstat_data->count,
					(unsigned long long)current_stat_data.residency,
					(unsigned long long)current_stat_data.count);
				return -1;
			}

		} else {
			/*
			 * For all power states that target power domain level
			 * > `target_pwrlvl, the previous residency and count
			 * should never be equal to current.
			 */
			if ((pstat_data->residency != current_stat_data.residency) ||
					(pstat_data->count != current_stat_data.count)) {
				ERROR("Imprecise validation failed for pwrlvl >"
						" target_pwrlvl. Stats for CPU"
						" %d for power state 0x%x. Prev"
						" stats 0x%llx 0x%llx, current stats"
						" 0x%llx 0x%llx\n",
						cpu_idx, power_state,
						(unsigned long long)pstat_data->residency,
						(unsigned long long)pstat_data->count,
						(unsigned long long)current_stat_data.residency,
						(unsigned long long)current_stat_data.count);
				return -1;
			}
		}
	} while (1);

	return 0;
}

/*
 * This function populates the stats for all power states at all power domain
 * levels for the current CPU in the global `stat_data` array.
 */
static void populate_all_stats_all_lvls(void)
{
	unsigned int pstateid_idx[PLAT_MAX_PWR_LEVEL + 1];
	int ret;
	unsigned int pwrlvl, susp_type, state_id, power_state;
	psci_stat_data_t *pstat_data;
	u_register_t mpidr = read_mpidr_el1() & MPID_MASK;
	int cpu_idx = platform_get_core_pos(mpidr);

	INIT_PWR_LEVEL_INDEX(pstateid_idx);
	do {
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, pstateid_idx);
		if (pstateid_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		/* Check if the power state is valid */
		ret = tftf_get_pstate_vars(&pwrlvl,
					&susp_type,
					&state_id,
					pstateid_idx);
		if (ret != PSCI_E_SUCCESS)
			continue;

		assert(pwrlvl <= PLAT_MAX_PWR_LEVEL);

		power_state = tftf_make_psci_pstate(pwrlvl,
				susp_type, state_id);

		pstat_data = get_psci_stat_data(cpu_idx, pwrlvl, pstateid_idx);
		pstat_data->residency = tftf_psci_stat_residency(mpidr,
							power_state);
		pstat_data->count = tftf_psci_stat_count(mpidr, power_state);
	} while (1);
}

/*
 * The core function by executed by all CPUs when `test_psci_stat_all_power_states`
 * test is executed. Each CPU queries the next valid power state using the
 * `power state` helpers and assumes that the power state progresses from lower
 * power levels to higher levels. It also assumes that the number of applicable
 * low power states are same across Big - Little clusters. In future this
 * assumption may not be true and this test may need to be reworked to have
 * `power domain` awareness. The sequence executed by the test is as follows:
 *
 * 1. Populate the stats for all power states at all power domain levels for
 *    the current CPU.
 *
 * 2. Each CPU queries the next applicable low power state using the
 *   `power state` helpers.
 *
 * 3. A synchronization point is created for all the CPUs taking part in the
 *    test using TFTF events. This is needed because low power states at higher
 *    power domain levels (like cluster) can only be achieved if all the CPUs
 *    within the power domain request the same low power state at the (nearly)
 *    same time.
 *
 * 4. Validate the current stats with the previously cached stats (done in
 *    Step 1).
 *
 * 5. Iterate for the next applicable power state.
 */
static test_result_t test_psci_stat(void)
{
	int ret;
	unsigned int pstateid_idx[PLAT_MAX_PWR_LEVEL + 1];
	unsigned int pwrlvl, state_id, power_state, susp_type, cpu_node;
	u_register_t mpidr = read_mpidr_el1() & MPID_MASK;
	int cpu_idx = platform_get_core_pos(mpidr);

	/* Initialize the per-cpu synchronization event */
	for_each_cpu(cpu_node) {
		u_register_t target_mpid;
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		tftf_init_event(&stat_sync[platform_get_core_pos(target_mpid)][cpu_idx]);
	}

	INIT_PWR_LEVEL_INDEX(pstateid_idx);

	do {
		/* Get the next power state */
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, pstateid_idx);
		if (pstateid_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		/* Populate the PSCI STATs for all power levels and all states */
		populate_all_stats_all_lvls();

		/* Check if the power state is valid */
		ret = tftf_get_pstate_vars(&pwrlvl, &susp_type, &state_id,
					pstateid_idx);
		if (ret != PSCI_E_SUCCESS)
			continue;

		power_state = tftf_make_psci_pstate(pwrlvl, susp_type, state_id);

		/*
		 * Create a synchronization point. A 2-D event array is used to
		 * signal every CPU by each CPU. This caters for the fact some
		 * CPUs may have higher performance than others and will not
		 * cause the sychronization to fail.
		 */
		for_each_cpu(cpu_node) {
			unsigned int target_idx;
			target_idx = platform_get_core_pos(
					tftf_get_mpidr_from_node(cpu_node));
			tftf_send_event(&stat_sync[target_idx][cpu_idx]);
			tftf_wait_for_event(&stat_sync[cpu_idx][target_idx]);
		}

		/*
		 * Initialize the cpu_count to zero for synchronizing
		 * participating CPUs.
		 */
		spin_lock(&cpu_count_lock);
		if (cpu_count == participating_cpu_count)
			cpu_count = 0;
		spin_unlock(&cpu_count_lock);

		ret = tftf_program_timer_and_suspend(PLAT_SUSPEND_ENTRY_TIME,
						     power_state, NULL, NULL);
		tftf_cancel_timer();
		if (ret) {
			ERROR("PSCI-STAT: Suspend failed. "
					"mpidr:0x%llx  pwr_lvl:0x%x  powerstate:0x%x\n",
					(unsigned long long)mpidr,
					pwrlvl, power_state);
			return TEST_RESULT_FAIL;
		}


		INFO("PSCI-STAT: mpidr:0x%llx  pwr_lvl:0x%x  powerstate:0x%x\n",
				(unsigned long long)mpidr,
				pwrlvl, power_state);

		wait_for_participating_cpus();

		ret = validate_stat_result(pstateid_idx, pwrlvl);
		if (ret)
			return TEST_RESULT_FAIL;
	} while (1);

	/*
	 * Populate the PSCI STATs for all power levels and all states
	 * for final validation from lead CPU
	 */
	populate_all_stats_all_lvls();

	return TEST_RESULT_SUCCESS;
}

/*
 * This function validates the stats for all secondary CPUs from the lead
 * CPU. It queries the stats for the power states for other CPUs and compares
 * it against the stats previous cached by them.
 */
static int validate_stat_result_from_lead(u_register_t target_cpu)
{
	unsigned int pwrlvl, susp_type, state_id, power_state, cpu_idx;
	int ret;
	psci_stat_data_t target_stat_data;
	psci_stat_data_t *pstat_data;
	unsigned int local_pstateid_idx[PLAT_MAX_PWR_LEVEL + 1];

	cpu_idx = platform_get_core_pos(target_cpu);
	INIT_PWR_LEVEL_INDEX(local_pstateid_idx);

	do {
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, local_pstateid_idx);
		if (local_pstateid_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		/* Check if the power state is valid */
		ret = tftf_get_pstate_vars(&pwrlvl,
					&susp_type,
					&state_id,
					local_pstateid_idx);
		if (ret != PSCI_E_SUCCESS)
			continue;

		assert(pwrlvl <= PLAT_MAX_PWR_LEVEL);

		power_state = tftf_make_psci_pstate(pwrlvl,
				susp_type, state_id);

		/* Get target CPU stat values for the power state */
		target_stat_data.residency = tftf_psci_stat_residency(target_cpu, power_state);
		target_stat_data.count = tftf_psci_stat_count(target_cpu, power_state);

		pstat_data = get_psci_stat_data(cpu_idx, pwrlvl, local_pstateid_idx);
		if ((pstat_data->residency != target_stat_data.residency) ||
				(pstat_data->count != target_stat_data.count)) {
			INFO("Stats for CPU %d for power state 0x%x :"
					" Recorded stats 0x%llx 0x%llx,"
					" Target stats 0x%llx 0x%llx\n",
					cpu_idx, power_state,
					(unsigned long long)pstat_data->residency,
					(unsigned long long)pstat_data->count,
					(unsigned long long)target_stat_data.residency,
					(unsigned long long)target_stat_data.count);
			return -1;
		}
	} while (1);

	return 0;
}


/*
 * @Test_Aim@ Verify if PSCI Stat Count and Residency are updated
 * correctly for all valid suspend states for every power domain at
 * various power levels. The `test_psci_stat()` invoked by all the CPUs
 * participating in the test. The final stats are also validated from the
 * lead CPU to ensure that stats queried from current CPU and another (lead)
 * CPU are the same.
 */
test_result_t test_psci_stat_all_power_states(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, ret;

	if (!is_psci_stat_supported())
		return TEST_RESULT_SKIPPED;

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	/* Initialize participating CPU count */
	participating_cpu_count = tftf_get_total_cpus_count();
	init_spinlock(&cpu_count_lock);

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);

		if (lead_mpid == target_mpid)
			continue;

		ret = tftf_cpu_on(target_mpid, (uintptr_t) test_psci_stat, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
					(unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}
	}

	if (test_psci_stat() != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	INFO("Validating stats from lead CPU\n");

	/* Validate the stat results from the lead CPU */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);

		if (lead_mpid == target_mpid)
			continue;

		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
				PSCI_STATE_OFF)
			;

		ret = validate_stat_result_from_lead(target_mpid);
		if (ret)
			return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Helper function for the secondary CPUs to boot and populate its stats
 * and power OFF.
 */
static test_result_t update_stats_and_power_off(void)
{
	wait_for_participating_cpus();

	populate_all_stats_all_lvls();
	return TEST_RESULT_SUCCESS;
}

/* The target level for the stats validation */
static int verify_stats_target_lvl;

/*
 * This is lighter validation of stat results than `validate_stat_result()`.
 * This function iterates over all the power states of type
 * `PSTATE_TYPE_POWERDOWN` for the current CPU and ensures that stats
 * corresponding to at least a single power state targeted to a power level
 * <= `verify_stats_target_lvl` have incremented as expected. If the stats
 * haven't incremented corresponding to a power state, then they must be
 * equal to the previous stats or else return FAILURE.
 */
static test_result_t verify_powerdown_stats(void)
{
	int ret;
	unsigned int stateid_idx[PLAT_MAX_PWR_LEVEL + 1];
	unsigned int pwrlvl, susp_type, state_id, power_state;
	psci_stat_data_t *pstat_data, curr_stat_data;
	u_register_t mpidr = read_mpidr_el1() & MPID_MASK;
	int cpu_idx = platform_get_core_pos(mpidr);

	test_result_t result = TEST_RESULT_FAIL;

	INIT_PWR_LEVEL_INDEX(stateid_idx);

	wait_for_participating_cpus();

	do {
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, stateid_idx);
		if (stateid_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		/* Check if the power state is valid */
		ret = tftf_get_pstate_vars(&pwrlvl,
					&susp_type,
					&state_id,
					stateid_idx);
		if ((ret != PSCI_E_SUCCESS) ||
				(susp_type != PSTATE_TYPE_POWERDOWN))
			continue;

		power_state = tftf_make_psci_pstate(pwrlvl,
					susp_type, state_id);
		pstat_data = get_psci_stat_data(cpu_idx, pwrlvl, stateid_idx);
		curr_stat_data.residency = tftf_psci_stat_residency(mpidr,
							power_state);
		curr_stat_data.count = tftf_psci_stat_count(mpidr, power_state);

		if ((curr_stat_data.count == (pstat_data->count + 1)) &&
				(curr_stat_data.residency >= pstat_data->residency)) {
			/*
			 * If the stats for at least a single power state
			 * targeted to a pwrlvl <= `verify_stats_target_lvl`
			 * satisfy the condition then set result to SUCCESS
			 */
			if (verify_stats_target_lvl >= pwrlvl)
				result = TEST_RESULT_SUCCESS;
		} else if ((curr_stat_data.count != pstat_data->count) ||
			(curr_stat_data.residency != pstat_data->residency)) {

			/*
			 * If the stats havent incremented, then they should be
			 * equal to previous.
			 */
			ERROR("Stats for CPU %d for power state 0x%x :"
				" Recorded stats 0x%llx 0x%llx,"
				" current stats 0x%llx 0x%llx\n",
				cpu_idx, power_state,
				(unsigned long long)pstat_data->residency,
				(unsigned long long)pstat_data->count,
				(unsigned long long)curr_stat_data.residency,
				(unsigned long long)curr_stat_data.count);

			return TEST_RESULT_FAIL;
		}
	} while (1);

	return result;
}

/*
 * @Test_Aim@ Validate PSCI stats after calling CPU_OFF on each secondary core.
 * The test sequence is as follows:
 *
 * 1. Invoke CPU_ON on all secondary cores which wakeup and update
 *    the stats corresponding to all power states.
 *
 * 2. After secondaries have turned OFF, suspend the lead CPU for a short time
 *    duration.
 *
 * 3. On wakeup from suspend, turn ON all secondaries which then validate the
 *    stats by invoking `verify_powerdown_stats()`.
 */
test_result_t test_psci_stats_cpu_off(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, off_cpu_count = 0, ret;
	unsigned int power_state, stateid;

	if (!is_psci_stat_supported())
		return TEST_RESULT_SKIPPED;

	SKIP_TEST_IF_LESS_THAN_N_CPUS(2);

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	/*
	 * The primary CPU is an external observer in this test.
	 * Count it out of the participating CPUs pool.
	 */
	participating_cpu_count = tftf_get_total_cpus_count() - 1;
	init_spinlock(&cpu_count_lock);
	cpu_count = 0;

	/* Turn on each secondary and update the stats. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);

		if (lead_mpid == target_mpid)
			continue;

		/*
		 * cpu_count will be updated by the secondary CPUs when they
		 * execute `update_stats_and_power_off` function.
		 */
		ret = tftf_cpu_on(target_mpid,
				(uintptr_t) update_stats_and_power_off, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx",
					(unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}

	}

	/* Wait for the secondary to turn OFF */
	for_each_cpu(cpu_node) {

		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
					PSCI_STATE_OFF)
			;
		off_cpu_count++;
	}

	assert(off_cpu_count == participating_cpu_count);
	cpu_count = 0;

	ret = tftf_psci_make_composite_state_id(MPIDR_AFFLVL0,
					PSTATE_TYPE_STANDBY, &stateid);
	if (ret != PSCI_E_SUCCESS) {
		ERROR("Failed to construct composite state\n");
		return TEST_RESULT_FAIL;
	}

	power_state = tftf_make_psci_pstate(MPIDR_AFFLVL0,
						PSTATE_TYPE_STANDBY, stateid);
	ret = tftf_program_timer_and_suspend(PLAT_SUSPEND_ENTRY_TIME,
						power_state, NULL, NULL);
	if (ret != 0) {
		ERROR("Failed to program timer or suspend CPU: 0x%x\n", ret);
		return TEST_RESULT_FAIL;
	}

	tftf_cancel_timer();

	/* That target level for CPU OFF is 0, (CPU power domain level) */
	verify_stats_target_lvl = 0;

	/* Now turn on each secondary and verify the stats */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);

		if (lead_mpid == target_mpid)
			continue;

		ret = tftf_cpu_on(target_mpid,
				(uintptr_t) verify_powerdown_stats, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx",
					(unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}
	}

	/* Wait for the secondary to turn OFF */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
					PSCI_STATE_OFF)
			;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Validate PSCI stats after SYSTEM SUSPEND.
 * The test sequence is as follows:
 *
 * 1. Invoke CPU_ON on all secondary cores which wakeup and update
 *    the stats corresponding to all power states.
 *
 * 2. After secondaries have turned OFF, invoke SYSTEM SUSPEND on the lead CPU.
 *
 * 3. On wakeup from suspend, turn ON all secondaries which then validate the
 *    stats by invoking `verify_powerdown_stats()`.
 */
test_result_t test_psci_stats_system_suspend(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, off_cpu_count = 0;
	int ret;

	ret = tftf_get_psci_feature_info(SMC_PSCI_SYSTEM_SUSPEND64);
	if (ret == PSCI_E_NOT_SUPPORTED) {
		tftf_testcase_printf("SYSTEM_SUSPEND not supported"
				" in EL3 firmware\n");
		return TEST_RESULT_SKIPPED;
	}

	if (!is_psci_stat_supported())
		return TEST_RESULT_SKIPPED;

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	/* Initialize participating CPU count. The lead CPU is excluded in the count */
	participating_cpu_count = tftf_get_total_cpus_count() - 1;
	init_spinlock(&cpu_count_lock);
	cpu_count = 0;

	/* Turn on each secondary and update the stats. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);

		if (lead_mpid == target_mpid)
			continue;

		/*
		 * cpu_count will be updated by the secondary CPUs when they
		 * execute `update_stats_and_power_off` function.
		 */
		ret = tftf_cpu_on(target_mpid,
				(uintptr_t) update_stats_and_power_off, 0);
		if (ret != PSCI_E_SUCCESS)
			return TEST_RESULT_FAIL;
	}

	/* Wait for the secondary to turn OFF */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
					PSCI_STATE_OFF)
			;

		off_cpu_count++;
	}

	assert(off_cpu_count == participating_cpu_count);
	cpu_count = 0;

	/* Update the stats corresponding to the lead CPU as well */
	populate_all_stats_all_lvls();

	/* Program timer to fire after delay and issue system suspend */
	ret = tftf_program_timer_and_sys_suspend(PLAT_SUSPEND_ENTRY_TIME,
								NULL, NULL);
	tftf_cancel_timer();
	if (ret)
		return TEST_RESULT_FAIL;

	/* That target level for SYSTEM SUSPEND is PLAT_MAX_PWR_LEVEL */
	verify_stats_target_lvl = PLAT_MAX_PWR_LEVEL;

	/* Now turn on each secondary CPU and verify the stats */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);

		if (lead_mpid == target_mpid)
			continue;

		ret = tftf_cpu_on(target_mpid,
				(uintptr_t) verify_powerdown_stats, 0);
		if (ret != PSCI_E_SUCCESS)
			return TEST_RESULT_FAIL;
	}

	/* Wait for the secondary CPU to turn OFF */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
					PSCI_STATE_OFF)
			;
	}

	/* Increment the participating CPU count to include the lead CPU as well */
	participating_cpu_count++;

	/* Verify the stats on the lead CPU as well */
	return verify_powerdown_stats();
}

/*
 * This function verifies the stats for all power states after a cold boot.
 * Basically the stats should be zero immediately after cold boot.
 *
 * As part of its initialization, the test framework puts the primary
 * CPU in CPU standby state (in order to detect the power state format).
 * This directly affects the PSCI statistics for the primary CPU.
 * This means that, when entering this test function, the CPU Standby
 * state statistics for the primary CPU may no longer be zero.
 * Thus, we just ignore it and don't test it.
 */
static test_result_t verify_psci_stats_cold_boot(void)
{
	int ret, cpu_node;
	unsigned int stateid_idx[PLAT_MAX_PWR_LEVEL + 1];
	unsigned int pwrlvl, susp_type, state_id, power_state;
	u_register_t target_mpid, lead_cpu, stat_count, stat_residency;

	lead_cpu = read_mpidr_el1() & MPID_MASK;

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);

		INIT_PWR_LEVEL_INDEX(stateid_idx);
		do {
			tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, stateid_idx);
			if (stateid_idx[0] == PWR_STATE_INIT_INDEX)
				break;

			/* Check if the power state is valid */
			ret = tftf_get_pstate_vars(&pwrlvl,
						&susp_type,
						&state_id,
						stateid_idx);
			if (ret != PSCI_E_SUCCESS)
				continue;

			if ((target_mpid == lead_cpu) && (pwrlvl == 0) &&
					(susp_type == PSTATE_TYPE_STANDBY))
				continue;

			power_state = tftf_make_psci_pstate(pwrlvl,
					susp_type, state_id);
			stat_residency = tftf_psci_stat_residency(target_mpid, power_state);
			stat_count =  tftf_psci_stat_count(target_mpid, power_state);
			if (stat_count || stat_residency) {
				ERROR("mpid = %lld, power_state = %x, "
				      "stat count = %lld, residency = %lld\n",
				      (unsigned long long) target_mpid,
				      power_state,
				      (unsigned long long) stat_count,
				      (unsigned long long) stat_residency);
				return TEST_RESULT_FAIL;
			}
		} while (1);
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Validate PSCI stats for each valid composite
 * power state after system shutdown
 */
test_result_t test_psci_stats_after_shutdown(void)
{
	smc_args args = { SMC_PSCI_SYSTEM_OFF };

	if (!is_psci_stat_supported())
		return TEST_RESULT_SKIPPED;

	if (tftf_is_rebooted()) {
		/*
		 * Successfully resumed from system off. Verify cold
		 * boot stats.
		 */
		return verify_psci_stats_cold_boot();
	}

	tftf_notify_reboot();
	tftf_smc(&args);

	/* The PSCI SYSTEM_OFF call is not supposed to return */
	tftf_testcase_printf("System didn't shutdown properly\n");
	return TEST_RESULT_FAIL;
}

/*
 * @Test_Aim@ Validate PSCI stats for each valid composite
 * power state after system reset
 */
test_result_t test_psci_stats_after_reset(void)
{
	smc_args args = { SMC_PSCI_SYSTEM_RESET };

	if (!is_psci_stat_supported())
		return TEST_RESULT_SKIPPED;

	if (tftf_is_rebooted()) {
		/*
		 * Successfully resumed from system reset. Verify cold
		 * boot stats.
		 */
		return verify_psci_stats_cold_boot();
	}

	tftf_notify_reboot();
	tftf_smc(&args);

	/* The PSCI SYSTEM_RESET call is not supposed to return */
	tftf_testcase_printf("System didn't reset properly\n");
	return TEST_RESULT_FAIL;
}
