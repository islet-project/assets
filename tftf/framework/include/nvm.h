/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __NVM_H__
#define __NVM_H__

#ifndef __ASSEMBLY__
#include <stddef.h>
#include <tftf.h>
#include "tests_list.h"

#define TEST_BUFFER_SIZE 0x80

typedef struct {
	/*
	 * @brief Last executed TFTF build message which consists of date and
	 * time when TFTF is built.
	 *
	 * A mismatch with the build message of currently executing binary will
	 * determine whether TFTF data structures stored in NVM needs to be
	 * initialised or not.
	 */
	char build_message[BUILD_MESSAGE_SIZE];

	/*
	 * The following 2 fields track the progress in the test session. They
	 * indicate which test case we are dealing with and the progress of this
	 * test, i.e. whether it hasn't started yet, or it is being executed
	 * right now, ...
	 */
	test_ref_t		test_to_run;
	test_progress_t		test_progress;

	/*
	 * @brief Scratch buffer for test internal use.
	 *
	 * A buffer that the test can use as a scratch area for whatever it is
	 * doing.
	 */
	char testcase_buffer[TEST_BUFFER_SIZE];

	/*
	 * @brief Results of tests.
	 *
	 * @note TESTCASE_RESULT_COUNT is defined in tests_list.h
	 * (auto-generated file).
	 */
	TESTCASE_RESULT testcase_results[TESTCASE_RESULT_COUNT];

	/*
	 * @brief Size of \a result_buffer.
	 */
	unsigned result_buffer_size;

	/*
	 * Buffer containing the output of all tests.
	 * Each test appends its output to the end of \a result_buffer.
	 * Tests which produce no output write nothing in \a result_buffer.
	 */
	 char *result_buffer;
} tftf_state_t;

/*
 * Helper macros to access fields of \a tftf_state_t structure.
 */
#define TFTF_STATE_OFFSET(_field)		offsetof(tftf_state_t, _field)

/*
 * Return 1 if we need to start a new test session;
 *        0 if we need to resume an interrupted session.
 */
unsigned int new_test_session(void);

/*
 * @brief Initialize NVM if necessary.
 *
 * When TFTF is launched on the target, its data structures need
 * to be initialised in NVM. However if some test resets the board
 * (as part of its normal behaviour or because it crashed) then
 * TFTF data structure must be left unchanged in order to resume
 * the test session where it has been left.
 *
 * This function detects whether TFTF has just been launched and if so
 * initialises its data structures. If TFTF has just reset then it does
 * nothing.
 *
 * @return STATUS_SUCCESS on success, another status code on failure.
 */
STATUS tftf_init_nvm(void);

/*
 * @brief Clean NVM.
 *
 * Clean TFTF data structures in NVM.
 * This function must be called when all tests have completed.
 *
 * @return STATUS_SUCCESS on success, another status code on failure.
 */
STATUS tftf_clean_nvm(void);

/* Writes the buffer to the flash at offset with length equal to
 * size
 * Returns: STATUS_FAIL, STATUS_SUCCESS, STATUS_OUT_OF_RESOURCES
 */
STATUS tftf_nvm_write(unsigned long long offset, const void *buffer, size_t size);

/* Reads the flash into buffer at offset with length equal to
 * size
 * Returns: STATUS_FAIL, STATUS_SUCCESS, STATUS_OUT_OF_RESOURCES
 */
STATUS tftf_nvm_read(unsigned long long offset, void *buffer, size_t size);
#endif /*__ASSEMBLY__*/

#endif
