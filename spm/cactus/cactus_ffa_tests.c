/*
 * Copyright (c) 2018-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include "ffa_helpers.h"
#include <sp_helpers.h>

/* FFA version test helpers */
#define FFA_MAJOR 1U
#define FFA_MINOR 0U

void ffa_tests(void)
{
	const char *test_ffa = "FFA Interfaces";
	const char *test_ffa_version = "FFA Version interface";

	announce_test_section_start(test_ffa);

	announce_test_start(test_ffa_version);

	smc_ret_values ret = ffa_version(MAKE_FFA_VERSION(FFA_MAJOR, FFA_MINOR));
	uint32_t spm_version = (uint32_t)(0xFFFFFFFF & ret.ret0);

	bool ffa_version_compatible = ((spm_version >> FFA_VERSION_MAJOR_SHIFT) == FFA_MAJOR &&
				       (FFA_VERSION_MINOR_MASK & spm_version) >= FFA_MINOR);

	NOTICE("FFA_VERSION returned %u.%u; Compatible: %i\n",
		spm_version >> FFA_VERSION_MAJOR_SHIFT,
		spm_version & FFA_VERSION_MINOR_MASK,
		(int)ffa_version_compatible);

	expect((int)ffa_version_compatible, (int)true);

	announce_test_end(test_ffa_version);

	announce_test_section_end(test_ffa);
}
