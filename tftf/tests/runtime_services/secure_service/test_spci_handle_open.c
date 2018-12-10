/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <cactus_def.h>
#include <debug.h>
#include <events.h>
#include <plat_topology.h>
#include <platform.h>
#include <smccc.h>
#include <spci_helpers.h>
#include <spci_svc.h>
#include <test_helpers.h>
#include <tftf_lib.h>

/*
 * @Test_Aim@ This tests that we can get the handle of a Secure Service and
 * close it correctly.
 */
test_result_t test_spci_handle_open(void)
{
	int ret;
	uint16_t handle1, handle2;

	/**********************************************************************
	 * Verify that SPCI is there and that it has the correct version.
	 **********************************************************************/

	SKIP_TEST_IF_SPCI_VERSION_LESS_THAN(0, 1);

	/**********************************************************************
	 * Try to get handle of an invalid Secure Service.
	 **********************************************************************/

	ret = spci_service_handle_open(TFTF_SPCI_CLIENT_ID, &handle1,
				       CACTUS_INVALID_UUID);

	if (ret != SPCI_NOT_PRESENT) {
		tftf_testcase_printf("%d: SPM should have returned SPCI_NOT_PRESENT. Returned: %d\n",
				     __LINE__, ret);
		return TEST_RESULT_FAIL;
	}

	/**********************************************************************
	 * Get handle of valid Secure Services.
	 **********************************************************************/

	ret = spci_service_handle_open(TFTF_SPCI_CLIENT_ID, &handle1,
				       CACTUS_SERVICE1_UUID);

	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to return a valid handle. Returned: %d\n",
				     __LINE__, ret);
		return TEST_RESULT_FAIL;
	}

	ret = spci_service_handle_open(TFTF_SPCI_CLIENT_ID, &handle2,
				       CACTUS_SERVICE2_UUID);

	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to return a valid handle. Returned: %d\n",
				     __LINE__, ret);
		return TEST_RESULT_FAIL;
	}

	/**********************************************************************
	 * Close invalid handle.
	 **********************************************************************/

	ret = spci_service_handle_close(TFTF_SPCI_CLIENT_ID, ~handle1);

	if (ret != SPCI_INVALID_PARAMETER) {
		tftf_testcase_printf("%d: SPM didn't fail to close the handle. Returned: %d\n",
				     __LINE__, ret);
		return TEST_RESULT_FAIL;
	}

	/**********************************************************************
	 * Close valid handles.
	 **********************************************************************/

	/* Close in the reverse order to test that it can be done. */

	ret = spci_service_handle_close(TFTF_SPCI_CLIENT_ID, handle2);

	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to close the handle. Returned: %d\n",
				     __LINE__, ret);
		return TEST_RESULT_FAIL;
	}

	ret = spci_service_handle_close(TFTF_SPCI_CLIENT_ID, handle1);

	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to close the handle. Returned: %d\n",
				     __LINE__, ret);
		return TEST_RESULT_FAIL;
	}

	/**********************************************************************
	 * All tests passed.
	 **********************************************************************/

	return TEST_RESULT_SUCCESS;
}
