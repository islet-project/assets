/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>

#include <arch_features.h>
#include <debug.h>
#include <host_realm_helper.h>
#include <host_realm_mem_layout.h>
#include <host_shared_data.h>

#define SLEEP_TIME_MS	200U
/*
 * @Test_Aim@ Test realm payload creation and execution
 */
test_result_t test_realm_create_enter(void)
{
	bool ret1, ret2;
	u_register_t retrmm;

	if (get_armv9_2_feat_rme_support() == 0U) {
		INFO("platform doesn't support RME\n");
		return TEST_RESULT_SKIPPED;
	}

	retrmm = rmi_version();
	VERBOSE("RMM version is: %lu.%lu\n",
			RMI_ABI_VERSION_GET_MAJOR(retrmm),
			RMI_ABI_VERSION_GET_MINOR(retrmm));
	/*
	 * Skip the test if RMM is TRP, TRP version is always null.
	 */
	if (retrmm == 0UL) {
		INFO("Test case not supported for TRP as RMM\n");
		return TEST_RESULT_SKIPPED;
	}

	if (!host_create_realm_payload((u_register_t)REALM_IMAGE_BASE,
			(u_register_t)PAGE_POOL_BASE,
			(u_register_t)(PAGE_POOL_MAX_SIZE +
			NS_REALM_SHARED_MEM_SIZE),
			(u_register_t)PAGE_POOL_MAX_SIZE)) {
		return TEST_RESULT_FAIL;
	}
	if (!host_create_shared_mem(NS_REALM_SHARED_MEM_BASE,
			NS_REALM_SHARED_MEM_SIZE)) {
		return TEST_RESULT_FAIL;
	}

	realm_shared_data_set_host_val(HOST_SLEEP_INDEX, SLEEP_TIME_MS);
	ret1 = host_enter_realm_execute(REALM_SLEEP_CMD);
	ret2 = host_destroy_realm();

	if (!ret1 || !ret2) {
		ERROR("test_realm_create_enter create:%d destroy:%d\n",
		ret1, ret2);
		return TEST_RESULT_FAIL;
	}
	return TEST_RESULT_SUCCESS;
}
