/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <quark_def.h>
#include <spci_helpers.h>
#include <test_helpers.h>

/*
 * @Test_Aim@ This tests opens a Secure Service handle and performs a simple
 * request to Quark to verify that its memory map is correct and that it is
 * working as expected.
 */
test_result_t test_quark_request(void)
{
	int ret;
	uint16_t handle_quark;
	u_register_t rx1, rx2, rx3;
	test_result_t result = TEST_RESULT_SUCCESS;

	SKIP_TEST_IF_SPCI_VERSION_LESS_THAN(0, 1);

	/* Open handles. */

	ret = spci_service_handle_open(TFTF_SPCI_CLIENT_ID, &handle_quark,
				       QUARK_SERVICE1_UUID);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to return a valid handle. Returned: 0x%x\n",
				     __LINE__, (uint32_t)ret);
		return TEST_RESULT_FAIL;
	}

	/* Send request to Quark */

	ret = spci_service_request_blocking(QUARK_GET_MAGIC,
					    0, 0, 0, 0, 0,
					    TFTF_SPCI_CLIENT_ID,
					    handle_quark,
					    &rx1, &rx2, &rx3);

	if (ret == SPCI_SUCCESS) {
		if (rx1 != QUARK_MAGIC_NUMBER) {
			tftf_testcase_printf("%d: Quark returned 0x%x 0x%lx 0x%lx 0x%lx\n",
					__LINE__, (uint32_t)ret, rx1, rx2, rx3);
			result = TEST_RESULT_FAIL;
		}
	} else {
		tftf_testcase_printf("%d: SPM should have returned SPCI_SUCCESS. Returned: 0x%x\n",
					__LINE__, (uint32_t)ret);
		result = TEST_RESULT_FAIL;
	}

	/* Close handle */

	ret = spci_service_handle_close(TFTF_SPCI_CLIENT_ID, handle_quark);
	if (ret != SPCI_SUCCESS) {
		tftf_testcase_printf("%d: SPM failed to close the handle. Returned: 0x%x\n",
				     __LINE__, (uint32_t)ret);
		result = TEST_RESULT_FAIL;
	}

	return result;
}
