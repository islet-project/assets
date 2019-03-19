/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <sp_helpers.h>
#include <spm_svc.h>
#include <stdint.h>

#include "cactus_mm.h"
#include "cactus_mm_tests.h"

/*
 * Miscellaneous SPM tests.
 */
void misc_tests(void)
{
	int32_t ret;

	const char *test_sect_desc = "miscellaneous";

	announce_test_section_start(test_sect_desc);

	const char *test_version = "SPM version check";

	announce_test_start(test_version);
	svc_args svc_values = { SPM_VERSION_AARCH32 };
	ret = sp_svc(&svc_values);
	INFO("Version = 0x%x (%u.%u)\n", ret,
	     (ret >> 16) & 0x7FFF, ret & 0xFFFF);
	expect(ret, SPM_VERSION_COMPILED);
	announce_test_end(test_version);

	announce_test_section_end(test_sect_desc);
}
