/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <nvm.h>
#include <platform.h>
#include <spinlock.h>
#include <stdarg.h>
#include <string.h>

/*
 * Temporary buffer to store 1 test output.
 * This will eventually be saved into NVM at the end of the execution
 * of this test.
 */
static char testcase_output[TESTCASE_OUTPUT_MAX_SIZE];
/*
 * A test output can be written in several pieces by calling
 * tftf_testcase_printf() multiple times. testcase_output_idx keeps the position
 * of the last character written in testcase_output buffer and allows to easily
 * append a new string at next call to tftf_testcase_printf().
 */
static unsigned int testcase_output_idx;

/* Lock to avoid concurrent accesses to the testcase output buffer */
static spinlock_t testcase_output_lock;

static tftf_state_t tftf_init_state = {
	.build_message		= "",
	.test_to_run		= {
		.testsuite_idx	= 0,
		.testcase_idx	= 0,
	},
	.test_progress		= TEST_READY,
	.testcase_buffer	= { 0 },
	.testcase_results	= {
		{
			.result		= TEST_RESULT_NA,
			.duration	= 0,
			.output_offset	= 0,
			.output_size	= 0,
		}
	},
	.result_buffer_size	= 0,
	.result_buffer		= NULL,
};

unsigned int new_test_session(void)
{
/* NEW_TEST_SESSION == 1 => we always want to start a new session */
#if NEW_TEST_SESSION
	INFO("Always starting a new test session (NEW_TEST_SESSION == 1)\n");
	return 1;
#endif
	char saved_build_msg[BUILD_MESSAGE_SIZE];

	/*
	 * Check the validity of the build message stored in NVM.
	 * It is invalid when it doesn't match with the TFTF binary currently
	 * executing.
	 */
	tftf_nvm_read(TFTF_STATE_OFFSET(build_message), saved_build_msg,
		BUILD_MESSAGE_SIZE);
	return !!strncmp(build_message, saved_build_msg, BUILD_MESSAGE_SIZE);
}

STATUS tftf_init_nvm(void)
{
	INFO("Initialising NVM\n");

	/* Copy the build message to identify the TFTF */
	strncpy(tftf_init_state.build_message, build_message, BUILD_MESSAGE_SIZE);
	return tftf_nvm_write(0, &tftf_init_state, sizeof(tftf_init_state));
}

STATUS tftf_clean_nvm(void)
{
	unsigned char corrupt_build_message = '\0';

	/*
	 * This will cause TFTF to re-initialise its data structures next time
	 * it runs.
	 */
	return tftf_nvm_write(TFTF_STATE_OFFSET(build_message),
			&corrupt_build_message,
			sizeof(corrupt_build_message));
}

STATUS tftf_set_test_to_run(const test_ref_t test_to_run)
{
	return tftf_nvm_write(TFTF_STATE_OFFSET(test_to_run), &test_to_run,
			sizeof(test_to_run));
}

STATUS tftf_get_test_to_run(test_ref_t *test_to_run)
{
	assert(test_to_run != NULL);
	return tftf_nvm_read(TFTF_STATE_OFFSET(test_to_run), test_to_run,
			sizeof(*test_to_run));
}

STATUS tftf_set_test_progress(test_progress_t test_progress)
{
	return tftf_nvm_write(TFTF_STATE_OFFSET(test_progress), &test_progress,
			sizeof(test_progress));
}

STATUS tftf_get_test_progress(test_progress_t *test_progress)
{
	assert(test_progress != NULL);
	return tftf_nvm_read(TFTF_STATE_OFFSET(test_progress), test_progress,
			sizeof(*test_progress));
}

STATUS tftf_testcase_set_result(const test_case_t *testcase,
				test_result_t result,
				unsigned long long duration)
{
	STATUS status;
	unsigned result_buffer_size = 0;
	TESTCASE_RESULT test_result;

	assert(testcase != NULL);

	/* Initialize Test case result */
	test_result.result = result;
	test_result.duration = duration;
	test_result.output_offset = 0;
	test_result.output_size = strlen(testcase_output);

	/* Does the test have an output? */
	if (test_result.output_size != 0) {
		/* Get the size of the buffer containing all tests outputs */
		status = tftf_nvm_read(TFTF_STATE_OFFSET(result_buffer_size),
				&result_buffer_size, sizeof(unsigned));
		if (status != STATUS_SUCCESS)
			goto reset_test_output;

		/*
		 * Write the output buffer at the end of the string buffer in
		 * NVM
		 */
		test_result.output_offset = result_buffer_size;
		status = tftf_nvm_write(
			TFTF_STATE_OFFSET(result_buffer) + result_buffer_size,
			testcase_output, test_result.output_size + 1);
		if (status != STATUS_SUCCESS)
			goto reset_test_output;

		/* And update the buffer size into NVM */
		result_buffer_size += test_result.output_size + 1;
		status = tftf_nvm_write(TFTF_STATE_OFFSET(result_buffer_size),
					&result_buffer_size, sizeof(unsigned));
		if (status != STATUS_SUCCESS)
			goto reset_test_output;
	}

	/* Write the test result into NVM */
	status = tftf_nvm_write(TFTF_STATE_OFFSET(testcase_results) +
				(testcase->index * sizeof(TESTCASE_RESULT)),
				&test_result, sizeof(TESTCASE_RESULT));

reset_test_output:
	/* Reset test output buffer for the next test */
	testcase_output_idx = 0;
	testcase_output[0] = 0;

	return status;
}

STATUS tftf_testcase_get_result(const test_case_t *testcase,
				TESTCASE_RESULT *result,
				char *test_output)
{
	STATUS status;
	unsigned output_size;

	assert(testcase != NULL);
	assert(result != NULL);
	assert(test_output != NULL);

	status = tftf_nvm_read(TFTF_STATE_OFFSET(testcase_results)
			+ (testcase->index * sizeof(TESTCASE_RESULT)),
			result, sizeof(TESTCASE_RESULT));
	if (status != STATUS_SUCCESS) {
		return status;
	}

	output_size = result->output_size;

	if (output_size != 0) {
		status = tftf_nvm_read(TFTF_STATE_OFFSET(result_buffer)
				+ result->output_offset,
				test_output, output_size);
		if (status != STATUS_SUCCESS)
			return status;
	}

	test_output[output_size] = 0;

	return STATUS_SUCCESS;
}

int tftf_testcase_printf(const char *format, ...)
{
	va_list ap;
	int available;
	int written = -1;

	spin_lock(&testcase_output_lock);

	assert(sizeof(testcase_output) >= testcase_output_idx);
	available = sizeof(testcase_output) - testcase_output_idx;
	if (available == 0) {
		ERROR("%s: Output buffer is full ; the string won't be printed.\n",
			__func__);
		ERROR("%s: Consider increasing TESTCASE_OUTPUT_MAX_SIZE value.\n",
			__func__);
		goto release_lock;
	}

	va_start(ap, format);
	written = vsnprintf(&testcase_output[testcase_output_idx], available,
			format, ap);
	va_end(ap);

	if (written < 0) {
		ERROR("%s: Output error (%d)", __func__, written);
		goto release_lock;
	}
	/*
	 * If vsnprintf() truncated the string due to the size limit passed as
	 * an argument then its return value is the number of characters (not
	 * including the trailing '\0') which would have been written to the
	 * final string if enough space had been available. Thus, a return value
	 * of size or more means that the output was truncated.
	 *
	 * Adjust the value of 'written' to reflect what has been actually
	 * written.
	 */
	if (written >= available) {
		ERROR("%s: String has been truncated (%u/%u bytes written).\n",
			__func__, available - 1, written);
		ERROR("%s: Consider increasing TESTCASE_OUTPUT_MAX_SIZE value.\n",
			__func__);
		written = available - 1;
	}

	/*
	 * Update testcase_output_idx to point to the '\0' of the buffer.
	 * The next call of tftf_testcase_printf() will overwrite '\0' to
	 * append its new string to the buffer.
	 */
	testcase_output_idx += written;

release_lock:
	spin_unlock(&testcase_output_lock);
	return written;
}

void tftf_notify_reboot(void)
{
#if DEBUG
	/* This function must be called by tests, not by the framework */
	test_progress_t test_progress;
	tftf_get_test_progress(&test_progress);
	assert(test_progress == TEST_IN_PROGRESS);
#endif /* DEBUG */

	VERBOSE("Test intends to reset\n");
	tftf_set_test_progress(TEST_REBOOTING);
}
