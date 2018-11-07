/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <platform_def.h>  /* For TESTCASE_OUTPUT_MAX_SIZE */
#include <stdio.h>
#include <string.h>
#include <tftf.h>

static unsigned int total_tests;
static unsigned int tests_stats[TEST_RESULT_MAX];

static void tftf_update_tests_statistics(test_result_t result)
{
	assert(TEST_RESULT_IS_VALID(result));
	total_tests++;
	tests_stats[result]++;
}

static const char *test_result_strings[TEST_RESULT_MAX] = {
	"Skipped", "Passed", "Failed", "Crashed",
};

const char *test_result_to_string(test_result_t result)
{
	assert(TEST_RESULT_IS_VALID(result));
	return test_result_strings[result];
}

void tftf_report_generate(void)
{
	unsigned i, j;
	const test_case_t *testcases;
	TESTCASE_RESULT testcase_result;
	char test_output[TESTCASE_OUTPUT_MAX_SIZE];
	STATUS status;

	/* Extract the result of all the testcases */
	printf("========== TEST REPORT ==========\n");
	for (i = 0; testsuites[i].name != NULL; i++) {
		printf("# Test suite '%s':\n", testsuites[i].name);
		testcases = testsuites[i].testcases;

		for (j = 0; testcases[j].name != NULL; j++) {
			status = tftf_testcase_get_result(&testcases[j], &testcase_result, test_output);
			if (status != STATUS_SUCCESS) {
				printf("Failed to get test result.\n");
				continue;
			}

			tftf_update_tests_statistics(testcase_result.result);
			/* TODO: print test duration */
			printf("\t - %s: %s\n", testcases[j].name,
				test_result_to_string(testcase_result.result));

			if (strlen(test_output) != 0) {
				printf("--- output ---\n");
				printf("%s", test_output);
				printf("--------------\n");
			}
		}
	}
	printf("=================================\n");

	for (i = TEST_RESULT_MIN; i < TEST_RESULT_MAX; i++) {
		printf("Tests %-8s: %d\n",
			test_result_to_string(i), tests_stats[i]);
	}
	printf("%-14s: %d\n", "Total tests", total_tests);
	printf("=================================\n");
}
