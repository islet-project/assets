/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psci.h>
#include <smccc.h>
#include <std_svc.h>
#include <tftf_lib.h>
#include <uuid_utils.h>

/*
 * Standard service UUID as returned by the implementation in the Trusted
 * Firmware.
 */
static const uuid_t armtf_std_svc_uuid = {
	0x108d905b, 0xf863, 0x47e8, 0xae, 0x2d,
	{ 0xc0, 0xfb, 0x56, 0x41, 0xf6, 0xe2 }
};

/**
 * @Test_Aim@ Query the Standard Service
 *
 * This test targets the implementation of the Standard Service in the Trusted
 * Firmware. If it is interfaced with a different implementation then this test
 * will most likely fail because the values returned by the service won't be the
 * ones expected.
 *
 * The following queries are performed:
 * 1) Call UID
 * 2) Call count
 * 3) Call revision details
 */
test_result_t test_query_std_svc(void)
{
	smc_args std_svc_args;
	smc_ret_values ret;
	uuid_t std_svc_uuid;
	char uuid_str[UUID_STR_SIZE];
	test_result_t test_result = TEST_RESULT_SUCCESS;

	/* Standard Service Call UID */
	std_svc_args.fid = SMC_STD_SVC_UID;
	ret = tftf_smc(&std_svc_args);

	make_uuid_from_4words(&std_svc_uuid,
			ret.ret0, ret.ret1, ret.ret2, ret.ret3);
	if (!uuid_equal(&std_svc_uuid, &armtf_std_svc_uuid)) {
		tftf_testcase_printf("Wrong UUID: expected %s,\n",
				uuid_to_str(&armtf_std_svc_uuid, uuid_str));
		tftf_testcase_printf("                 got %s\n",
				uuid_to_str(&std_svc_uuid, uuid_str));
		test_result = TEST_RESULT_FAIL;
	}

	/* Standard Service Call Count */
	std_svc_args.fid = SMC_STD_SVC_CALL_COUNT;
	ret = tftf_smc(&std_svc_args);

	if (ret.ret0 == SMC_UNKNOWN) {
		tftf_testcase_printf("Querying STD service call count"
				" failed\n");
		test_result = TEST_RESULT_FAIL;
	} else {
		tftf_testcase_printf("STD Service Call Count reported by firmware:"
			" %llu\n", (unsigned long long)ret.ret0);
	}

	/* Standard Service Call Revision details */
	std_svc_args.fid = SMC_STD_SVC_REVISION;
	ret = tftf_smc(&std_svc_args);

	if ((ret.ret0 != STD_SVC_REVISION_MAJOR) ||
	    (ret.ret1 != STD_SVC_REVISION_MINOR)) {
		tftf_testcase_printf(
			"Wrong Revision: expected {%u.%u}, got {%llu.%llu}\n",
			STD_SVC_REVISION_MAJOR, STD_SVC_REVISION_MINOR,
			(unsigned long long)ret.ret0,
			(unsigned long long)ret.ret1);
		test_result = TEST_RESULT_FAIL;
	}

	return test_result;
}
