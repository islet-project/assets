/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <sp_helpers.h>
#include <spm_svc.h>
#include <sprt_client.h>
#include <sprt_svc.h>
#include <types.h>

#include "cactus.h"
#include "cactus_tests.h"

/*
 * Miscellaneous SPM tests.
 */
void misc_tests(void)
{
	int32_t ret;

	const char *test_sect_desc = "miscellaneous";

	announce_test_section_start(test_sect_desc);

	const char *test_version_sprt = "SPRT version check";

	announce_test_start(test_version_sprt);
	ret = sprt_version();
	INFO("Version = 0x%x (%u.%u)\n", ret,
	     (ret >> SPRT_VERSION_MAJOR_SHIFT) & SPRT_VERSION_MAJOR_MASK,
	     ret & SPRT_VERSION_MINOR_MASK);
	expect(ret, SPRT_VERSION_COMPILED);
	announce_test_end(test_version_sprt);

	announce_test_section_end(test_sect_desc);
}
