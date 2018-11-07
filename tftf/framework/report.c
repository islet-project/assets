/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <stdio.h>
#include <stdbool.h>
#include <tftf.h>

static const char *test_result_strings[TEST_RESULT_MAX] = {
	"Skipped", "Passed", "Failed", "Crashed",
};

static const char *test_result_to_string(test_result_t result)
{
	assert(TEST_RESULT_IS_VALID(result));
	return test_result_strings[result];
}

void print_testsuite_start(const test_suite_t *testsuite)
{
	mp_printf("--\n");
	mp_printf("Running test suite '%s'\n", testsuite->name);
	mp_printf("Description: %s\n", testsuite->description);
	mp_printf("\n");
}

void print_test_start(const test_case_t *test)
{
	mp_printf("> Executing '%s'\n", test->name);
}

void print_test_end(const test_case_t *test)
{
	TESTCASE_RESULT result;
	char output[TESTCASE_OUTPUT_MAX_SIZE];

	tftf_testcase_get_result(test, &result, output);

	mp_printf("  TEST COMPLETE %54s\n",
		  test_result_to_string(result.result));
	if (strlen(output) != 0) {
		mp_printf("%s", output);
	}
	mp_printf("\n");
}

void print_tests_summary(void)
{
	int total_tests = 0;
	int tests_stats[TEST_RESULT_MAX] = { 0 };

	mp_printf("******************************* Summary *******************************\n");

	/* Go through the list of test suites. */
	for (int i = 0; testsuites[i].name != NULL; i++) {
		bool passed = true;

		mp_printf("> Test suite '%s'\n", testsuites[i].name);

		const test_case_t *testcases = testsuites[i].testcases;

		/* Go through the list of tests inside this test suite. */
		for (int j = 0; testcases[j].name != NULL; j++) {
			TESTCASE_RESULT result;
			char output[TESTCASE_OUTPUT_MAX_SIZE];

			if (tftf_testcase_get_result(&testcases[j], &result,
					output) != STATUS_SUCCESS) {
				mp_printf("Failed to get test result.\n");
				continue;
			}

			assert(TEST_RESULT_IS_VALID(result.result));

			/*
			 * Consider that a test suite passed if all of its
			 * tests passed or were skipped.
			 */
			if ((result.result != TEST_RESULT_SUCCESS) &&
			    (result.result != TEST_RESULT_SKIPPED)) {
				passed = false;
			}

			total_tests++;
			tests_stats[result.result]++;
		}
		mp_printf("%70s\n", passed ? "Passed" : "Failed");
	}

	mp_printf("=================================\n");

	for (int i = TEST_RESULT_MIN; i < TEST_RESULT_MAX; i++) {
		mp_printf("Tests %-8s: %d\n",
			test_result_to_string(i), tests_stats[i]);
	}
	mp_printf("%-14s: %d\n", "Total tests", total_tests);
	mp_printf("=================================\n");
}
