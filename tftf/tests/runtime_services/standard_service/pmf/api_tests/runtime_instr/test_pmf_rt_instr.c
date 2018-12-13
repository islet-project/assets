/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <plat_topology.h>
#include <platform.h>
#include <pmf.h>
#include <power_management.h>
#include <psci.h>
#include <smccc.h>
#include <string.h>
#include <sys/errno.h>
#include <tftf_lib.h>
#include <timer.h>

#define TOTAL_IDS		6
#define ENTER_PSCI		0
#define EXIT_PSCI		1
#define ENTER_HW_LOW_PWR	2
#define EXIT_HW_LOW_PWR		3
#define ENTER_CFLUSH		4
#define EXIT_CFLUSH		5

static spinlock_t cpu_count_lock;
static volatile int cpu_count;
static volatile int participating_cpu_count;
static u_register_t timestamps[PLATFORM_CORE_COUNT][TOTAL_IDS];
static unsigned int target_pwrlvl;

/* Helper function to wait for CPUs participating in the test. */
static void wait_for_participating_cpus(void)
{
	assert(participating_cpu_count <= PLATFORM_CORE_COUNT);

	spin_lock(&cpu_count_lock);
	cpu_count++;
	spin_unlock(&cpu_count_lock);

	assert(cpu_count <= PLATFORM_CORE_COUNT);

	while (cpu_count != participating_cpu_count)
		continue;
}

/*
 * Perform an SMC call into TF-A to collect timestamp specified by `tid`
 * and pass it as a parameter back to the caller.
 */
static u_register_t pmf_get_ts(u_register_t tid, u_register_t *v)
{
	smc_args args = { 0 };
	smc_ret_values ret;

	args.fid = PMF_SMC_GET_TIMESTAMP;
	args.arg1 = tid;
	args.arg2 = read_mpidr_el1();
	ret = tftf_smc(&args);
	*v = ret.ret1;
	return ret.ret0;
}

static int cycles_to_ns(uint64_t cycles, uint64_t freq, uint64_t *ns)
{
	if (cycles > UINT64_MAX / 1000000000 || freq == 0)
		return -ERANGE;
	*ns = cycles * 1000000000 / freq;
	return 0;
}

static u_register_t *get_core_timestamps(void)
{
	unsigned int pos = platform_get_core_pos(read_mpidr_el1());

	assert(pos < PLATFORM_CORE_COUNT);
	return timestamps[pos];
}

/* Check timestamps for the suspend/cpu off tests. */
static test_result_t check_pwr_down_ts(void)
{
	u_register_t *ts;

	ts = get_core_timestamps();
	if (!(ts[ENTER_PSCI] <= ts[ENTER_HW_LOW_PWR] &&
	    ts[ENTER_HW_LOW_PWR] <= ts[EXIT_HW_LOW_PWR] &&
	    ts[EXIT_HW_LOW_PWR] <= ts[EXIT_PSCI])) {
		tftf_testcase_printf("PMF timestamps are not correctly ordered\n");
		return TEST_RESULT_FAIL;
	}

	if (ts[ENTER_CFLUSH] > ts[EXIT_CFLUSH]) {
		tftf_testcase_printf("PMF timestamps are not correctly ordered\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Capture all runtime instrumentation timestamps for the current
 * CPU and store them into the timestamps array.
 */
static test_result_t get_ts(void)
{
	u_register_t tid, *ts;
	int i;

	ts = get_core_timestamps();
	for (i = 0; i < TOTAL_IDS; i++) {
		tid = PMF_ARM_TIF_IMPL_ID << PMF_IMPL_ID_SHIFT;
		tid |= PMF_RT_INSTR_SVC_ID << PMF_SVC_ID_SHIFT | i;
		if (pmf_get_ts(tid, &ts[i]) != 0) {
			ERROR("Failed to capture PMF timestamp\n");
			return TEST_RESULT_FAIL;
		}
	}
	return TEST_RESULT_SUCCESS;
}

/* Dump suspend statistics for the suspend/cpu off test. */
static int dump_suspend_stats(const char *func_name)
{
	u_register_t *ts;
	u_register_t target_mpid;
	uint64_t freq, cycles[3], period[3];
	int cpu_node, ret;
	unsigned int pos;

	freq = read_cntfrq_el0();

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		pos = platform_get_core_pos(target_mpid);
		assert(pos < PLATFORM_CORE_COUNT);
		ts = timestamps[pos];

		cycles[0] = ts[ENTER_HW_LOW_PWR] - ts[ENTER_PSCI];
		ret = cycles_to_ns(cycles[0], freq, &period[0]);
		if (ret < 0) {
			ERROR("cycles_to_ns: out of range\n");
			return TEST_RESULT_FAIL;
		}

		cycles[1] = ts[EXIT_PSCI] - ts[EXIT_HW_LOW_PWR];
		ret = cycles_to_ns(cycles[1], freq, &period[1]);
		if (ret < 0) {
			ERROR("cycles_to_ns: out of range\n");
			return TEST_RESULT_FAIL;
		}

		cycles[2] = ts[EXIT_CFLUSH] - ts[ENTER_CFLUSH];
		ret = cycles_to_ns(cycles[2], freq, &period[2]);
		if (ret < 0) {
			ERROR("cycles_to_ns: out of range\n");
			return TEST_RESULT_FAIL;
		}

		printf("<RT_INSTR:%s\t%llu\t%llu\t%02llu\t%02llu\t%02llu/>\n", func_name,
		    (unsigned long long)MPIDR_AFF_ID(target_mpid, 1),
		    (unsigned long long)MPIDR_AFF_ID(target_mpid, 0),
		    (unsigned long long)period[0],
		    (unsigned long long)period[1],
		    (unsigned long long)period[2]);
	}

	return TEST_RESULT_SUCCESS;
}

/* Dump statistics for a PSCI version call. */
static int dump_psci_version_stats(const char *func_name)
{
	u_register_t *ts;
	u_register_t target_mpid;
	uint64_t freq, cycles, period;
	int cpu_node, ret;
	unsigned int pos;

	freq = read_cntfrq_el0();

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		pos = platform_get_core_pos(target_mpid);
		assert(pos < PLATFORM_CORE_COUNT);
		ts = timestamps[pos];

		cycles = ts[EXIT_PSCI] - ts[ENTER_PSCI];
		ret = cycles_to_ns(cycles, freq, &period);
		if (ret < 0) {
			ERROR("cycles_to_ns: out of range\n");
			return TEST_RESULT_FAIL;
		}

		printf("<RT_INSTR:%s\t%llu\t%llu\t%02llu/>\n", func_name,
		    (unsigned long long)MPIDR_AFF_ID(target_mpid, 1),
		    (unsigned long long)MPIDR_AFF_ID(target_mpid, 0),
		    (unsigned long long)period);
	}

	return TEST_RESULT_SUCCESS;
}

/* Dummy entry point to turn core off for the CPU off test. */
static test_result_t dummy_entrypoint(void)
{
	wait_for_participating_cpus();
	return TEST_RESULT_SUCCESS;
}

/* Entrypoint to collect timestamps for CPU off test. */
static test_result_t collect_ts_entrypoint(void)
{
	wait_for_participating_cpus();

	if (get_ts() != TEST_RESULT_SUCCESS ||
	    check_pwr_down_ts() != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
}

/* Suspend current core to power level specified by `target_pwrlvl`. */
static test_result_t suspend_current_core(void)
{
	unsigned int pstateid_idx[PLAT_MAX_PWR_LEVEL + 1];
	unsigned int pwrlvl, susp_type, state_id, power_state;
	int ret;

	INIT_PWR_LEVEL_INDEX(pstateid_idx);

	tftf_set_deepest_pstate_idx(target_pwrlvl, pstateid_idx);
	tftf_get_pstate_vars(&pwrlvl, &susp_type, &state_id, pstateid_idx);

	power_state = tftf_make_psci_pstate(pwrlvl, susp_type, state_id);

	ret = tftf_program_timer_and_suspend(PLAT_SUSPEND_ENTRY_TIME,
	    power_state, NULL, NULL);
	if (ret != 0) {
		ERROR("Failed to program timer or suspend CPU: 0x%x\n", ret);
		return TEST_RESULT_FAIL;
	}

	tftf_cancel_timer();

	return TEST_RESULT_SUCCESS;
}

/* This entrypoint is used for all suspend tests. */
static test_result_t suspend_core_entrypoint(void)
{
	wait_for_participating_cpus();

	if (suspend_current_core() != TEST_RESULT_SUCCESS ||
	    get_ts() != TEST_RESULT_SUCCESS ||
	    check_pwr_down_ts() != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
}

/* Entrypoint used for the PSCI version test. */
static test_result_t psci_version_entrypoint(void)
{
	u_register_t *ts;
	int version;

	wait_for_participating_cpus();

	version = tftf_get_psci_version();
	if (!tftf_is_valid_psci_version(version)) {
		tftf_testcase_printf(
			"Wrong PSCI version:0x%08x\n", version);
		return TEST_RESULT_FAIL;
	}

	if (get_ts() != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	/* Check timestamp order. */
	ts = get_core_timestamps();
	if (ts[ENTER_PSCI] > ts[EXIT_PSCI]) {
		tftf_testcase_printf("PMF timestamps are not correctly ordered\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/* Check if runtime instrumentation is enabled in TF. */
static int is_rt_instr_supported(void)
{
	u_register_t tid, dummy;

	tid = PMF_ARM_TIF_IMPL_ID << PMF_IMPL_ID_SHIFT;
	tid |= PMF_RT_INSTR_SVC_ID << PMF_SVC_ID_SHIFT;
	return !pmf_get_ts(tid, &dummy);
}

/*
 * This test powers on all on the non-lead cores and brings
 * them and the lead core to a common synchronization point.
 * Then a suspend to the deepest power level supported on the
 * platform is initiated on all cores in parallel.
 */
static test_result_t test_rt_instr_susp_parallel(const char *func_name)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, ret;

	if (is_rt_instr_supported() == 0)
		return TEST_RESULT_SKIPPED;

	lead_mpid = read_mpidr_el1() & MPID_MASK;
	participating_cpu_count = tftf_get_total_cpus_count();
	init_spinlock(&cpu_count_lock);
	cpu_count = 0;

	/* Power on all the non-lead cores. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)suspend_core_entrypoint, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
			    (unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}
	}

	if (suspend_core_entrypoint() != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	/* Wait for the non-lead cores to power down. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
		    PSCI_STATE_OFF)
			continue;
		cpu_count--;
	}

	cpu_count--;
	assert(cpu_count == 0);

	return dump_suspend_stats(func_name);
}

/*
 * This tests powers on each non-lead core in sequence and
 * suspends it to the deepest power level supported on the platform.
 * It then waits for the core to power off.  Each core in
 * the non-lead cluster will bring the entire clust down when it
 * powers off because it will be the only core active in the cluster.
 * The lead core will also be suspended in a similar fashion.
 */
static test_result_t test_rt_instr_susp_serial(const char *func_name)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, ret;

	if (is_rt_instr_supported() == 0)
		return TEST_RESULT_SKIPPED;

	lead_mpid = read_mpidr_el1() & MPID_MASK;
	participating_cpu_count = 1;
	init_spinlock(&cpu_count_lock);
	cpu_count = 0;

	/* Suspend one core at a time. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)suspend_core_entrypoint, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
			    (unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
		    PSCI_STATE_OFF)
			continue;
		cpu_count--;
	}

	assert(cpu_count == 0);

	/* Suspend lead core as well. */
	if (suspend_core_entrypoint() != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	cpu_count--;
	assert(cpu_count == 0);

	return dump_suspend_stats(func_name);
}

/*
 * @Test_Aim@ CPU suspend to deepest power level on all cores in parallel.
 *
 * This test should exercise contention in TF-A as all the cores initiate
 * a CPU suspend call in parallel.
 */
test_result_t test_rt_instr_susp_deep_parallel(void)
{
	target_pwrlvl = PLAT_MAX_PWR_LEVEL;
	/*
	 * The test name needs to be passed all the way down to
	 * the output functions to differentiate the results.
	 * Ditto, for all cases below.
	 */
	return test_rt_instr_susp_parallel(__func__);
}

/*
 * @Test_Aim@ CPU suspend on all cores in parallel.
 *
 * Suspend all cores in parallel to target power level 0.
 * Cache associated with power domain level 0 is flushed.  For
 * Juno, the L1 cache is flushed.
 */
test_result_t test_rt_instr_cpu_susp_parallel(void)
{
	target_pwrlvl = 0;
	return test_rt_instr_susp_parallel(__func__);
}

/*
 * @Test_Aim@ CPU suspend to deepest power level on all cores in sequence.
 *
 * Each core in the non-lead cluster brings down the entire cluster when
 * it goes down.
 */
test_result_t test_rt_instr_susp_deep_serial(void)
{
	target_pwrlvl = PLAT_MAX_PWR_LEVEL;
	return test_rt_instr_susp_serial(__func__);
}

/*
 * @Test_Aim@ CPU suspend on all cores in sequence.
 *
 * Cache associated with level 0 power domain are flushed.  For
 * Juno, the L1 cache is flushed.
 */
test_result_t test_rt_instr_cpu_susp_serial(void)
{
	target_pwrlvl = 0;
	return test_rt_instr_susp_serial(__func__);
}

/*
 * @Test_Aim@ CPU off on all non-lead cores in sequence and
 * suspend lead to deepest power level.
 *
 * The test sequence is as follows:
 *
 * 1) Turn on and turn off each non-lead core in sequence.
 * 2) Program wake up timer and suspend the lead core to deepest power level.
 * 3) Turn on each secondary core and get the timestamps from each core.
 *
 * All cores in the non-lead cluster bring the cluster
 * down when they go down.  Core 4 brings the big cluster down
 * when it goes down.
 */
test_result_t test_rt_instr_cpu_off_serial(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, ret;

	if (is_rt_instr_supported() == 0)
		return TEST_RESULT_SKIPPED;

	target_pwrlvl = PLAT_MAX_PWR_LEVEL;
	lead_mpid = read_mpidr_el1() & MPID_MASK;
	participating_cpu_count = 1;
	init_spinlock(&cpu_count_lock);
	cpu_count = 0;

	/* Turn core on/off one at a time. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)dummy_entrypoint, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
			    (unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
		    PSCI_STATE_OFF)
			continue;
		cpu_count--;
	}

	assert(cpu_count == 0);

	/* Suspend lead core as well. */
	if (suspend_core_entrypoint() != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	cpu_count--;
	assert(cpu_count == 0);

	/* Turn core on one at a time and collect timestamps. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)collect_ts_entrypoint, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
			    (unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
		    PSCI_STATE_OFF)
			continue;
		cpu_count--;
	}

	assert(cpu_count == 0);

	return dump_suspend_stats(__func__);
}

/*
 * @Test_Aim@ PSCI version call on all cores in parallel.
 */
test_result_t test_rt_instr_psci_version_parallel(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, ret;

	if (is_rt_instr_supported() == 0)
		return TEST_RESULT_SKIPPED;

	lead_mpid = read_mpidr_el1() & MPID_MASK;
	participating_cpu_count = tftf_get_total_cpus_count();
	init_spinlock(&cpu_count_lock);
	cpu_count = 0;

	/* Power on all the non-lead cores. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)psci_version_entrypoint, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x%llx\n",
			    (unsigned long long)target_mpid);
			return TEST_RESULT_FAIL;
		}
	}

	if (psci_version_entrypoint() != TEST_RESULT_SUCCESS)
		return TEST_RESULT_FAIL;

	/* Wait for the non-lead cores to power down. */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		if (lead_mpid == target_mpid)
			continue;
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
		    PSCI_STATE_OFF)
			continue;
		cpu_count--;
	}

	cpu_count--;
	assert(cpu_count == 0);

	return dump_psci_version_stats(__func__);
}
