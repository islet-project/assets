/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <arm_gic.h>
#include <assert.h>
#include <debug.h>
#include <irq.h>
#include <mmio.h>
#include <nvm.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <string.h>
#include <sys/types.h>
#include <tftf.h>
#include <tftf_lib.h>
#include <timer.h>

/* version information for TFTF */
extern const char version_string[];

unsigned int lead_cpu_mpid;

/* Defined in hotplug.c */
extern volatile test_function_t test_entrypoint[PLATFORM_CORE_COUNT];

/* Per-CPU results for the current test */
static test_result_t test_results[PLATFORM_CORE_COUNT];

/* Context ID passed to tftf_psci_cpu_on() */
static u_register_t cpu_on_ctx_id_arr[PLATFORM_CORE_COUNT];

static unsigned int test_is_rebooting;

static inline const test_suite_t *current_testsuite(void)
{
	test_ref_t test_to_run;
	tftf_get_test_to_run(&test_to_run);
	return &testsuites[test_to_run.testsuite_idx];
}

static inline const test_case_t *current_testcase(void)
{
	test_ref_t test_to_run;
	tftf_get_test_to_run(&test_to_run);
	return &testsuites[test_to_run.testsuite_idx].
		testcases[test_to_run.testcase_idx];
}

/*
 * Identify the next test in the tests list and update the NVM data to point to
 * that test.
 * If there is no more tests to execute, return NULL.
 * Otherwise, return the test case.
 */
static const test_case_t *advance_to_next_test(void)
{
	test_ref_t test_to_run;
	const test_case_t *testcase;
	unsigned int testcase_idx;
	unsigned int testsuite_idx;

#if DEBUG
	test_progress_t progress;
	tftf_get_test_progress(&progress);
	assert(progress == TEST_COMPLETE);
#endif

	tftf_get_test_to_run(&test_to_run);
	testcase_idx = test_to_run.testcase_idx;
	testsuite_idx = test_to_run.testsuite_idx;

	/* Move to the next test case in the current test suite */
	++testcase_idx;
	testcase = &testsuites[testsuite_idx].testcases[testcase_idx];

	if (testcase->name == NULL) {
		/*
		 * There's no more test cases in the current test suite so move
		 * to the first test case of the next test suite.
		 */
		const test_suite_t *testsuite;
		testcase_idx = 0;
		++testsuite_idx;
		testsuite = &testsuites[testsuite_idx];
		testcase = &testsuite->testcases[0];

		if (testsuite->name == NULL) {
			/*
			 * This was the last test suite so there's no more tests
			 * at all.
			 */
			return NULL;
		}
	}

	VERBOSE("Moving to test (%u,%u)\n", testsuite_idx, testcase_idx);
	test_to_run.testsuite_idx = testsuite_idx;
	test_to_run.testcase_idx = testcase_idx;
	tftf_set_test_to_run(test_to_run);
	tftf_set_test_progress(TEST_READY);

	return testcase;
}

/*
 * This function is executed only by the lead CPU.
 * It prepares the environment for the next test to run.
 */
static void prepare_next_test(void)
{
	unsigned int mpid;
	unsigned int core_pos;
	unsigned int cpu_node;

	/* This function should be called by the lead CPU only */
	assert((read_mpidr_el1() & MPID_MASK) == lead_cpu_mpid);

	/*
	 * Only the lead CPU should be powered on at this stage. All other CPUs
	 * should be powered off or powering off. If some CPUs are not powered
	 * off yet, wait for them to power off.
	 */
	for_each_cpu(cpu_node) {
		mpid = tftf_get_mpidr_from_node(cpu_node);
		if (mpid == lead_cpu_mpid)
			assert(tftf_is_cpu_online(mpid));
		else
			while (tftf_psci_affinity_info(mpid, MPIDR_AFFLVL0)
					  == PSCI_STATE_ON)
				;
	}

	/* No CPU should have entered the test yet */
	assert(tftf_get_ref_cnt() == 0);

	/* Populate the test entrypoint for the lead CPU */
	core_pos = platform_get_core_pos(lead_cpu_mpid);
	test_entrypoint[core_pos] = (test_function_t) current_testcase()->test;

	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; ++i)
		test_results[i] = TEST_RESULT_NA;

	/* If we're starting a new testsuite, announce it. */
	test_ref_t test_to_run;
	tftf_get_test_to_run(&test_to_run);
	if (test_to_run.testcase_idx == 0) {
		print_testsuite_start(current_testsuite());
	}

	print_test_start(current_testcase());

	/* Program the watchdog */
	tftf_platform_watchdog_set();

	/* TODO: Take a 1st timestamp to be able to measure test duration */

	tftf_set_test_progress(TEST_IN_PROGRESS);
}

/*
 * Go through individual CPUs' test results and determine the overall
 * test result from that.
 */
static test_result_t get_overall_test_result(void)
{
	test_result_t result = TEST_RESULT_NA;
	unsigned int cpu_mpid;
	unsigned int cpu_node;
	unsigned int core_pos;

	for_each_cpu(cpu_node) {
		cpu_mpid = tftf_get_mpidr_from_node(cpu_node);
		core_pos = platform_get_core_pos(cpu_mpid);

		switch (test_results[core_pos]) {
		case TEST_RESULT_NA:
			/* Ignoring */
			break;

		case TEST_RESULT_SKIPPED:
			/*
			 * If at least one CPU skipped the test, consider the
			 * whole test as skipped as well.
			 */
			return TEST_RESULT_SKIPPED;

		case TEST_RESULT_SUCCESS:
			result = TEST_RESULT_SUCCESS;
			break;

		case TEST_RESULT_FAIL:
			return TEST_RESULT_FAIL;

		case TEST_RESULT_CRASHED:
			/*
			 * Means the CPU never returned from the test whereas it
			 * was supposed to. Either there is a bug in the test's
			 * implementation or some sort of unexpected crash
			 * happened.
			 * If at least one CPU crashed, consider the whole test
			 * as crashed as well.
			 */
			return TEST_RESULT_CRASHED;

		default:
			ERROR("Unknown test result value: %u\n",
				test_results[core_pos]);
			panic();
		}
	}

	/*
	 * At least one CPU (i.e. the lead CPU) should have participated in the
	 * test.
	 */
	assert(result != TEST_RESULT_NA);
	return result;
}

/*
 * This function is executed by the last CPU to exit the test only.
 * It does the necessary bookkeeping and reports the overall test result.
 * If it was the last test, it will also generate the final test report.
 * Otherwise, it will reset the platform, provided that the platform
 * supports reset from non-trusted world. This ensures that the next test
 * runs in a clean environment
 *
 * Return 1 if this was the last test, 0 otherwise.
 */
static unsigned int close_test(void)
{
	const test_case_t *next_test;

#if DEBUG
	/*
	 * Check that the test didn't pretend resetting the platform, when in
	 * fact it returned into the framework.
	 *
	 * If that happens, the test implementation should be fixed.
	 * However, it is not a fatal error so just flag the problem in debug
	 * builds.
	 */
	test_progress_t progress;
	tftf_get_test_progress(&progress);
	assert(progress != TEST_REBOOTING);
#endif /* DEBUG */

	tftf_set_test_progress(TEST_COMPLETE);
	test_is_rebooting = 0;

	/* TODO: Take a 2nd timestamp and compute test duration */

	/* Reset watchdog */
	tftf_platform_watchdog_reset();

	/* Ensure no CPU is still executing the test */
	assert(tftf_get_ref_cnt() == 0);

	/* Save test result in NVM */
	tftf_testcase_set_result(current_testcase(),
				get_overall_test_result(),
				0);

	print_test_end(current_testcase());

	/* The test is finished, let's move to the next one (if any) */
	next_test = advance_to_next_test();

	/* If this was the last test then report all results */
	if (!next_test) {
		print_tests_summary();
		tftf_clean_nvm();
		return 1;
	} else {
#if (PLAT_SUPPORTS_NS_RESET && !NEW_TEST_SESSION && USE_NVM)
		/*
		 * Reset the platform so that the next test runs in a clean
		 * environment.
		 */
		INFO("Reset platform before executing next test:%p\n",
				(void *) &(next_test->test));
		tftf_plat_reset();
		bug_unreachable();
#endif
	}

	return 0;
}

/*
 * Hand over to lead CPU, i.e.:
 *  1) Power on lead CPU
 *  2) Power down calling CPU
 */
static void __dead2 hand_over_to_lead_cpu(void)
{
	int ret;
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	VERBOSE("CPU%u: Hand over to lead CPU%u\n", core_pos,
		platform_get_core_pos(lead_cpu_mpid));

	/*
	 * Power on lead CPU.
	 * The entry point address passed as the 2nd argument of tftf_cpu_on()
	 * doesn't matter because it will be overwritten by prepare_next_test().
	 * Pass a NULL pointer to easily catch the problem in case something
	 * goes wrong.
	 */
	ret = tftf_cpu_on(lead_cpu_mpid, 0, 0);
	if (ret != PSCI_E_SUCCESS) {
		ERROR("CPU%u: Failed to power on lead CPU%u (%d)\n",
			core_pos, platform_get_core_pos(lead_cpu_mpid), ret);
		panic();
	}

	/* Wait for lead CPU to be actually powered on */
	while (!tftf_is_cpu_online(lead_cpu_mpid))
		;

	/*
	 * Lead CPU has successfully booted, let's now power down the calling
	 * core.
	 */
	tftf_cpu_off();
	panic();
}

void __dead2 run_tests(void)
{
	unsigned int mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);
	unsigned int test_session_finished;
	unsigned int cpus_cnt;

	while (1) {
		if (mpid == lead_cpu_mpid && (tftf_get_ref_cnt() == 0))
			prepare_next_test();

		/*
		 * Increment the reference count to indicate that the CPU is
		 * participating in the test.
		 */
		tftf_inc_ref_cnt();

		/*
		 * Mark the CPU's test result as "crashed". This is meant to be
		 * overwritten by the actual test result when the CPU returns
		 * from the test function into the framework. In case the CPU
		 * crashes in the test (and thus, never returns from it), this
		 * variable will hold the right value.
		 */
		test_results[core_pos] = TEST_RESULT_CRASHED;

		/*
		 * Jump to the test entrypoint for this core.
		 * - For the lead CPU, it has been populated by
		 *   prepare_next_test()
		 * - For other CPUs, it has been populated by tftf_cpu_on() or
		 *   tftf_try_cpu_on()
		 */
		while (test_entrypoint[core_pos] == 0)
			;

		test_results[core_pos] = test_entrypoint[core_pos]();
		test_entrypoint[core_pos] = 0;

		/*
		 * Decrement the reference count to indicate that the CPU is not
		 * participating in the test any longer.
		 */
		cpus_cnt = tftf_dec_ref_cnt();

		/*
		 * Last CPU to exit the test gets to do the necessary
		 * bookkeeping and to report the overall test result.
		 * Other CPUs shut down.
		 */
		if (cpus_cnt == 0) {
			test_session_finished = close_test();
			if (test_session_finished)
				break;

			if (mpid != lead_cpu_mpid) {
				hand_over_to_lead_cpu();
				bug_unreachable();
			}
		} else {
			tftf_cpu_off();
			panic();
		}
	}

	tftf_exit();

	/* Should never reach this point */
	bug_unreachable();
}

u_register_t tftf_get_cpu_on_ctx_id(unsigned int core_pos)
{
	assert(core_pos < PLATFORM_CORE_COUNT);

	return cpu_on_ctx_id_arr[core_pos];
}

void tftf_set_cpu_on_ctx_id(unsigned int core_pos, u_register_t context_id)
{
	assert(core_pos < PLATFORM_CORE_COUNT);

	cpu_on_ctx_id_arr[core_pos] = context_id;
}

unsigned int tftf_is_rebooted(void)
{
	return test_is_rebooting;
}

/*
 * Return 0 if the test session can be resumed
 *        -1 otherwise.
 */
static int resume_test_session(void)
{
	test_ref_t test_to_run;
	test_progress_t test_progress;
	const test_case_t *next_test;

	/* Get back on our feet. Where did we stop? */
	tftf_get_test_to_run(&test_to_run);
	tftf_get_test_progress(&test_progress);
	assert(TEST_PROGRESS_IS_VALID(test_progress));

	switch (test_progress) {
	case TEST_READY:
		/*
		 * The TFTF has reset in the framework code, before the test
		 * actually started.
		 * Nothing to update, just start the test from scratch.
		 */
		break;

	case TEST_IN_PROGRESS:
		/*
		 * The test crashed, i.e. it couldn't complete.
		 * Update the test result in NVM then move to the next test.
		 */
		INFO("Test has crashed, moving to the next one\n");
		tftf_testcase_set_result(current_testcase(),
					TEST_RESULT_CRASHED,
					0);
		next_test = advance_to_next_test();
		if (!next_test) {
			INFO("No more tests\n");
			return -1;
		}
		break;

	case TEST_COMPLETE:
		/*
		 * The TFTF has reset in the framework code, after the test had
		 * completed but before we finished the framework maintenance
		 * required to move to the next test.
		 *
		 * In this case, we don't know the exact state of the data:
		 * maybe we had the time to update the test result,
		 * maybe we had the time to move to the next test.
		 * We can't be sure so let's stay on the safe side and just
		 * restart the test session from the beginning...
		 */
		NOTICE("The test framework has been interrupted in the middle "
			"of critical maintenance operations.\n");
		NOTICE("Can't recover execution.\n");
		return -1;

	case TEST_REBOOTING:
		/*
		 * Nothing to update about the test session, as we want to
		 * re-enter the same test. Just remember that the test is
		 * rebooting in case it queries this information.
		 */
		test_is_rebooting = 1;
		break;

	default:
		bug_unreachable();
	}

	return 0;
}

/*
 * C entry point in the TFTF.
 * This function is executed by the primary CPU only.
 */
void __dead2 tftf_cold_boot_main(void)
{
	STATUS status;
	int rc;

	NOTICE("%s\n", TFTF_WELCOME_STR);
	NOTICE("%s\n", build_message);
	NOTICE("%s\n\n", version_string);

#ifndef AARCH32
	NOTICE("Running at NS-EL%u\n", IS_IN_EL(1) ? 1 : 2);
#else
	NOTICE("Running in AArch32 HYP mode\n");
#endif

	tftf_arch_setup();
	tftf_platform_setup();
	tftf_init_topology();

	tftf_irq_setup();

	rc = tftf_initialise_timer();
	if (rc != 0) {
		ERROR("Failed to initialize the timer subsystem (%d).\n", rc);
		tftf_exit();
	}

	/* Enable the SGI used by the timer management framework */
	tftf_irq_enable(IRQ_WAKE_SGI, GIC_HIGHEST_NS_PRIORITY);
	enable_irq();

	if (new_test_session()) {
		NOTICE("Starting a new test session\n");
		status = tftf_init_nvm();
		if (status != STATUS_SUCCESS) {
			/*
			 * TFTF will have an undetermined behavior if its data
			 * structures have not been initialised. There's no
			 * point in continuing execution.
			 */
			ERROR("FATAL: Failed to initialise internal data structures in NVM.\n");
			tftf_clean_nvm();
			tftf_exit();
		}
	} else {
		NOTICE("Resuming interrupted test session\n");
		rc = resume_test_session();
		if (rc < 0) {
			print_tests_summary();
			tftf_clean_nvm();
			tftf_exit();
		}
	}

	/* Initialise the CPUs status map */
	tftf_init_cpus_status_map();

	/*
	 * Detect power state format and get power state information for
	 * a platform.
	 */
	tftf_init_pstate_framework();

	/* The lead CPU is always the primary core. */
	lead_cpu_mpid = read_mpidr_el1() & MPID_MASK;

	/*
	 * Hand over to lead CPU if required.
	 * If the primary CPU is not the lead CPU for the first test then:
	 *  1) Power on the lead CPU
	 *  2) Power down the primary CPU
	 */
	if ((read_mpidr_el1() & MPID_MASK) != lead_cpu_mpid) {
		hand_over_to_lead_cpu();
		bug_unreachable();
	}

	/* Enter the test session */
	run_tests();

	/* Should never reach this point */
	bug_unreachable();
}

void __dead2 tftf_exit(void)
{
	NOTICE("Exiting tests.\n");

	/* Let the platform code clean up if required */
	tftf_platform_end();

	while (1)
		wfi();
}
