/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <platform_def.h>  /* For TESTCASE_OUTPUT_MAX_SIZE */
#include <semihosting.h>
#include <stdio.h>
#include <string.h>
#include <tftf.h>

struct tftf_report_ops {
	long (*open)(const char *fname);
	void (*write)(long handle, const char *str);
	void (*close)(long handle);
};

#define TEST_REPORT_JUNIT_FILENAME "tftf_report_junit.xml"
#define TEST_REPORT_RAW_FILENAME "tftf_report_raw.txt"

#if defined(TEST_REPORT_UART_RAW) || defined(TEST_REPORT_UART_JUNIT)
static long tftf_report_uart_open(const char *fname)
{
	printf("********** %s **********\n", fname);
	return 0;
}

static void tftf_report_uart_write(long handle, const char *str)
{
	(void)handle;
	assert(str);
	/* Not using printf to avoid doing two copies. */
	while (*str) {
		putchar(*str++);
	}
}

static void tftf_report_uart_close(long handle)
{
	(void)handle;
	printf("************************\n");
}

const struct tftf_report_ops tftf_report_uart_ops = {
	.open = tftf_report_uart_open,
	.write = tftf_report_uart_write,
	.close = tftf_report_uart_close,
};
#endif /* defined(TEST_REPORT_UART_RAW) || defined(TEST_REPORT_UART_JUNIT) */

#if defined(TEST_REPORT_UART_RAW) || defined(TEST_REPORT_SEMIHOSTING_RAW)
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

static void tftf_report_generate_raw(const struct tftf_report_ops *rops,
					const char *fname)
{
#define WRITE(str) rops->write(file_handle, str)
#define BUFFER_SIZE 200
	unsigned i, j;
	long file_handle;
	char buffer[BUFFER_SIZE];
	const test_case_t *testcases;
	TESTCASE_RESULT testcase_result;
	char test_output[TESTCASE_OUTPUT_MAX_SIZE];
	STATUS status;

	file_handle = rops->open(fname);
	if (file_handle == -1)
		return;

	/* Extract the result of all the testcases */
	WRITE("========== TEST REPORT ==========\n");
	for (i = 0; testsuites[i].name != NULL; i++) {
		snprintf(buffer, BUFFER_SIZE, "# Test suite '%s':\n", testsuites[i].name);
		WRITE(buffer);
		testcases = testsuites[i].testcases;

		for (j = 0; testcases[j].name != NULL; j++) {
			status = tftf_testcase_get_result(&testcases[j], &testcase_result, test_output);
			if (status != STATUS_SUCCESS) {
				WRITE("Failed to get test result.\n");
				continue;
			}

			tftf_update_tests_statistics(testcase_result.result);
			/* TODO: print test duration */
			snprintf(buffer, BUFFER_SIZE, "\t - %s: %s\n", testcases[j].name,
				test_result_to_string(testcase_result.result));
			WRITE(buffer);

			if (strlen(test_output) != 0) {
				WRITE("--- output ---\n");
				snprintf(buffer, BUFFER_SIZE, "%s", test_output);
				WRITE(buffer);
				WRITE("--------------\n");
			}
		}
	}
	WRITE("=================================\n");

	for (i = TEST_RESULT_MIN; i < TEST_RESULT_MAX; i++) {
		snprintf(buffer, BUFFER_SIZE, "Tests %-8s: %d\n",
			test_result_to_string(i), tests_stats[i]);
		WRITE(buffer);
	}
	snprintf(buffer, BUFFER_SIZE, "%-14s: %d\n", "Total tests", total_tests);
	WRITE(buffer);
	WRITE("=================================\n");

	rops->close(file_handle);
#undef BUFFER_SIZE
#undef WRITE
}
#endif /* defined(TEST_REPORT_UART_RAW) || defined(TEST_REPORT_SEMIHOSTING_RAW) */

#if defined(TEST_REPORT_SEMIHOSTING_RAW) || defined(TEST_REPORT_SEMIHOSTING_JUNIT)
static long tftf_report_semihosting_open(const char *fname)
{
	/* Create the report on the semihosting */
	long handle = semihosting_file_open(fname, FOPEN_MODE_WPLUS);
	if (handle == -1) {
		ERROR("Failed to create test report file \"%s\" on semihosting"
			" [status = %ld].\n", fname, handle);
	}
	NOTICE("Opened file \"%s\" on semihosting with handle %ld.\n", fname, handle);
	return handle;
}

static void tftf_report_semihosting_write(long handle, const char *str)
{
	size_t length = strlen(str);
	semihosting_file_write(handle, &length, (const uintptr_t) str);
}

static void tftf_report_semihosting_close(long handle)
{
	semihosting_file_close(handle);
	NOTICE("Closing file with handle %ld on semihosting.\n", handle);
}

const struct tftf_report_ops tftf_report_semihosting_ops = {
	.open = tftf_report_semihosting_open,
	.write = tftf_report_semihosting_write,
	.close = tftf_report_semihosting_close,
};
#endif /* defined(TEST_REPORT_SEMIHOSTING_RAW) || defined(TEST_REPORT_SEMIHOSTING_JUNIT) */


#if defined(TEST_REPORT_UART_JUNIT) || defined(TEST_REPORT_SEMIHOSTING_JUNIT)
static void tftf_report_generate_junit(const struct tftf_report_ops *rops,
					const char *fname)
{
#define WRITE(str) rops->write(file_handle, str)
#define BUFFER_SIZE 200

	long file_handle;
	unsigned i, j;
	const test_case_t *testcases;
	TESTCASE_RESULT result;
	char buffer[BUFFER_SIZE];
	char test_output[TESTCASE_OUTPUT_MAX_SIZE];

	file_handle = rops->open(fname);

	if (file_handle == -1) {
		return;
	}
	WRITE("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	WRITE("<testsuites>\n");

	/* Extract the result of all the testcases */
	for (i = 0; testsuites[i].name != NULL; i++) {
		snprintf(buffer, BUFFER_SIZE, "<testsuite name=\"%s\">\n",
			testsuites[i].name);
		WRITE(buffer);
		testcases = testsuites[i].testcases;
		for (j = 0; testcases[j].name != NULL; j++) {
			tftf_testcase_get_result(&testcases[j], &result, test_output);

			snprintf(buffer, BUFFER_SIZE, "  <testcase name=\"%s\" time=\"%llu\"",
				testcases[j].name, result.duration);
			WRITE(buffer);
			if (result.result == TEST_RESULT_SUCCESS) {
				WRITE("/>\n");
			} else {
				WRITE(">\n");
				if (result.result == TEST_RESULT_SKIPPED) {
					WRITE("    <skipped/>\n");
				} else {
					WRITE("    <error type=\"failed\">\n");
					WRITE(test_output);
					WRITE("    </error>\n");
				}
				WRITE("  </testcase>\n");
			}
		}
		WRITE("</testsuite>\n");
	}

	WRITE("</testsuites>\n");
	rops->close(file_handle);
#undef BUFFER_SIZE
#undef WRITE
}
#endif /* defined(TEST_REPORT_UART_JUNIT) || defined(TEST_REPORT_SEMIHOSTING_JUNIT) */

void tftf_report_generate(void)
{
	int nb_reports = 0;
#ifdef TEST_REPORT_UART_RAW
	tftf_report_generate_raw(&tftf_report_uart_ops, "raw");
	nb_reports++;
#endif
#ifdef TEST_REPORT_UART_JUNIT
	tftf_report_generate_junit(&tftf_report_uart_ops, "junit");
	nb_reports++;
#endif
#ifdef TEST_REPORT_SEMIHOSTING_RAW
	tftf_report_generate_raw(&tftf_report_semihosting_ops,
				TEST_REPORT_RAW_FILENAME);
	nb_reports++;
#endif
#ifdef TEST_REPORT_SEMIHOSTING_JUNIT
	tftf_report_generate_junit(&tftf_report_semihosting_ops,
				TEST_REPORT_JUNIT_FILENAME);
	nb_reports++;
#endif
	assert(nb_reports > 0);
}
