/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <arch_helpers.h>
#include <stdbool.h>
#include <tftf_lib.h>

#ifndef AARCH32

/*
 * This function asserts that pointer authentication registers are accessible
 * from lower ELs. If not permitted from EL3, the access will cause a crash.
 */
test_result_t test_ptrauth_access(void)
{
	bool has_ptrauth = false;
	uint64_t id_aa64isar1 = read_id_aa64isar1_el1();

	has_ptrauth = has_ptrauth || ((id_aa64isar1 & ID_AA64ISAR1_GPI_MASK) != 0U);
	has_ptrauth = has_ptrauth || ((id_aa64isar1 & ID_AA64ISAR1_GPA_MASK) != 0U);
	has_ptrauth = has_ptrauth || ((id_aa64isar1 & ID_AA64ISAR1_API_MASK) != 0U);
	has_ptrauth = has_ptrauth || ((id_aa64isar1 & ID_AA64ISAR1_APA_MASK) != 0U);

	if (!has_ptrauth) {
		tftf_testcase_printf("Pointer authentication not supported.\n");
		return TEST_RESULT_SKIPPED;
	}

	(void) read_apgakeylo_el1();

	return TEST_RESULT_SUCCESS;
}

#else

test_result_t test_ptrauth_access(void)
{
	tftf_testcase_printf("Not supported on AArch32.\n");
	return TEST_RESULT_SKIPPED;
}

#endif
