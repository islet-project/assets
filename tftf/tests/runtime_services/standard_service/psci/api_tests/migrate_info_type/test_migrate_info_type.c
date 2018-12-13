/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <psci.h>
#include <smccc.h>
#include <tftf_lib.h>
#include <trusted_os.h>
#include <tsp.h>
#include <uuid_utils.h>

/*
 * @Test_Aim@ Exercise PSCI MIGRATE_INFO_TYPE API
 *
 * This test exercises the PSCI MIGRATE_INFO_TYPE API in the following 2
 * scenarios:
 *
 *   == No Trusted OS is present ==
 *   In this case,
 *   a) either the EL3 firmware doesn't implement the MIGRATE_INFO_TYPE call
 *   b) or the MIGRATE_INFO_TYPE call should report that the Trusted OS is
 *      not present.
 *   In both cases, the MIGRATE call should not be supported.
 *
 *   == A Trusted OS is present and it is the TSP ==
 *   In this case, the MIGRATE_INFO_TYPE call should report that the TSP is
 *   MP-capable and hence the MIGRATE call should not be supported.
 *
 * This test doesn't support any other Trusted OS than the TSP. It will be
 * skipped for any other TOS.
 */
test_result_t test_migrate_info_type(void)
{
	uuid_t tos_uuid;
	char tos_uuid_str[UUID_STR_SIZE];
	smc_args args;
	smc_ret_values ret;
	int32_t mp_support;
	int32_t migrate_ret;

	/* Identify the level of multicore support present in the Trusted OS */
	args.fid = SMC_PSCI_MIG_INFO_TYPE;
	ret = tftf_smc(&args);
	mp_support = (int32_t) ret.ret0;

	if (is_trusted_os_present(&tos_uuid)) {
		/* The only Trusted OS that this test supports is the TSP */
		if (!uuid_equal(&tos_uuid, &tsp_uuid)) {
			tftf_testcase_printf("Trusted OS is not the TSP, "
					"its UUID is: %s\n",
					uuid_to_str(&tos_uuid, tos_uuid_str));
			return TEST_RESULT_SKIPPED;
		}

		INFO("TSP detected\n");

		if (mp_support != TSP_MIGRATE_INFO) {
			tftf_testcase_printf(
				"Wrong return value for MIGRATE_INFO_TYPE: "
				"expected %i, got %i\n",
				TSP_MIGRATE_INFO, mp_support);
			return TEST_RESULT_FAIL;
		}
	} else {
		INFO("No Trusted OS detected\n");

		if ((mp_support != PSCI_E_NOT_SUPPORTED) &&
		    (mp_support != PSCI_TOS_NOT_PRESENT_MP)) {
			tftf_testcase_printf(
				"Wrong return value for MIGRATE_INFO_TYPE: "
				"expected %i or %i, got %i\n",
				PSCI_E_NOT_SUPPORTED,
				PSCI_TOS_NOT_PRESENT_MP,
				mp_support);
			return TEST_RESULT_FAIL;
		}
	}

	/*
	 * Either there is no Trusted OS or the Trusted OS is the TSP.
	 * In both cases, the MIGRATE call should not be supported.
	 */
	args.fid = SMC_PSCI_MIG;
	/*
	 * Pass a valid MPID so that the MIGRATE call doesn't fail because of
	 * invalid parameters
	 */
	args.arg1 = read_mpidr_el1() & MPID_MASK;
	ret = tftf_smc(&args);
	migrate_ret = (int32_t) ret.ret0;

	if (migrate_ret != PSCI_E_NOT_SUPPORTED) {
		tftf_testcase_printf("Wrong return value for MIGRATE: "
				"expected %i, got %i\n",
				PSCI_E_NOT_SUPPORTED, migrate_ret);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
