/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains tests that measure the round trip latency of an SMC.
 * The SMC calls used are simple ones (PSCI_VERSION and the Standard Service
 * UID) that involve almost no handling on the EL3 firmware's side so that we
 * come close to measuring the overhead of the SMC itself.
 */

#include <arch_helpers.h>
#include <arm_arch_svc.h>
#include <debug.h>
#include <psci.h>
#include <smccc.h>
#include <std_svc.h>
#include <string.h>
#include <tftf_lib.h>
#include <utils_def.h>

#define ITERATIONS_CNT 1000
static unsigned long long raw_results[ITERATIONS_CNT];

/* Latency information in nano-seconds */
struct latency_info {
	unsigned long long min;
	unsigned long long max;
	unsigned long long avg;
};

static inline unsigned long long cycles_to_ns(unsigned long long cycles)
{
	unsigned long long freq = read_cntfrq_el0();
	return (cycles * 1000000000) / freq;
}

/*
 * Send the given SMC 'ITERATIONS_CNT' times, measure the time it takes to
 * return back from the SMC call each time, and gather some statistics across
 * the whole series.
 *
 * The statistics consist of:
 *  - minimum latency (i.e the shortest duration across the whole series);
 *  - maximum latency (i.e the longest duration across the whole series);
 *  - average latency.
 *
 * These statistics are stored in the latency_info structure whose address
 * is passed as an argument.
 *
 * This function also prints some additional, intermediate information, like the
 * number of cycles for each SMC and the average number of cycles for an SMC
 * round trip.
 */
static void test_measure_smc_latency(const smc_args *smc_args,
				     struct latency_info *latency)
{
	unsigned long long cycles;
	unsigned long long min_cycles;
	unsigned long long max_cycles;
	unsigned long long avg_cycles;
	unsigned long long cycles_sum = 0;

	min_cycles = UINT64_MAX;
	max_cycles = 0;
	memset(raw_results, 0, sizeof(raw_results));

	for (unsigned int i = 0; i < ITERATIONS_CNT; ++i) {
		cycles = read_cntpct_el0();
		tftf_smc(smc_args);
		cycles = read_cntpct_el0() - cycles;

		min_cycles = MIN(min_cycles, cycles);
		max_cycles = MAX(max_cycles, cycles);

		cycles_sum += cycles;

		raw_results[i] = cycles;
	}

	avg_cycles = cycles_sum / ITERATIONS_CNT;
	tftf_testcase_printf("Average number of cycles: %llu\n",
		(unsigned long long) avg_cycles);
	latency->min = cycles_to_ns(min_cycles);
	latency->max = cycles_to_ns(max_cycles);
	latency->avg = cycles_to_ns(avg_cycles);

	NOTICE("Raw results:\n");
	for (unsigned int i = 0; i < ITERATIONS_CNT; ++i) {
		NOTICE("%llu cycles\t%llu ns\n",
			raw_results[i], cycles_to_ns(raw_results[i]));
	}
}

/*
 * Measure the latency of the PSCI_VERSION SMC and print the result.
 * This test always succeed.
 */
test_result_t smc_psci_version_latency(void)
{
	struct latency_info latency;
	smc_args args = { SMC_PSCI_VERSION };

	test_measure_smc_latency(&args, &latency);
	tftf_testcase_printf(
		"Average time: %llu ns (ranging from %llu to %llu)\n",
		(unsigned long long) latency.avg,
		(unsigned long long) latency.min,
		(unsigned long long) latency.max);

	return TEST_RESULT_SUCCESS;
}

/*
 * Measure the latency of the Standard Service Call UID SMC and print the
 * result.
 * This test always succeed.
 */
test_result_t smc_std_svc_call_uid_latency(void)
{
	struct latency_info latency;
	smc_args args = { SMC_STD_SVC_UID };

	test_measure_smc_latency(&args, &latency);
	tftf_testcase_printf(
		"Average time: %llu ns (ranging from %llu to %llu)\n",
		(unsigned long long) latency.avg,
		(unsigned long long) latency.min,
		(unsigned long long) latency.max);

	return TEST_RESULT_SUCCESS;
}

test_result_t smc_arch_workaround_1(void)
{
	struct latency_info latency;
	smc_args args;
	smc_ret_values ret;
	int32_t expected_ver;

	/* Check if SMCCC version is at least v1.1 */
	expected_ver = MAKE_SMCCC_VERSION(1, 1);
	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_VERSION;
	ret = tftf_smc(&args);
	if ((int32_t)ret.ret0 < expected_ver) {
		printf("Unexpected SMCCC version: 0x%x\n",
		       (int)ret.ret0);
		return TEST_RESULT_SKIPPED;
	}

	/* Check if SMCCC_ARCH_WORKAROUND_1 is implemented */
	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_ARCH_FEATURES;
	args.arg1 = SMCCC_ARCH_WORKAROUND_1;
	ret = tftf_smc(&args);
	if ((int)ret.ret0 == -1) {
		printf("SMCCC_ARCH_WORKAROUND_1 is not implemented\n");
		return TEST_RESULT_SKIPPED;
	}

	memset(&args, 0, sizeof(args));
	args.fid = SMCCC_ARCH_WORKAROUND_1;

	test_measure_smc_latency(&args, &latency);
	tftf_testcase_printf(
		"Average time: %llu ns (ranging from %llu to %llu)\n",
		(unsigned long long) latency.avg,
		(unsigned long long) latency.min,
		(unsigned long long) latency.max);

	return TEST_RESULT_SUCCESS;
}
