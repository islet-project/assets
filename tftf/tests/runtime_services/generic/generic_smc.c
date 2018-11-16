/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <psci.h>
#include <smccc.h>
#include <std_svc.h>
#include <string.h>
#include <tftf_lib.h>
#include <tsp.h>
#include <utils_def.h>

/* An invalid SMC function number. */
#define INVALID_FN 0x666

/* PSCI version returned by TF-A. */
static const uint32_t psci_version = PSCI_VERSION(PSCI_MAJOR_VER,
						  PSCI_MINOR_VER);

/* UUID of the standard service in TF-A. */
static const smc_ret_values std_svc_uuid = {
	0x108d905b, 0x47e8f863, 0xfbc02dae, 0xe2f64156
};

/*
 * Build an SMC function ID given its type (fast/yielding), calling convention,
 * owning entity number and function number.
 */
static inline uint32_t make_smc_fid(unsigned int type, unsigned int cc,
				    unsigned int oen, unsigned int func_num)
{
	return (type << FUNCID_TYPE_SHIFT) | (cc << FUNCID_CC_SHIFT)
		| (oen << FUNCID_OEN_SHIFT) | (func_num << FUNCID_NUM_SHIFT);
}

/* Exit the test if the specified condition holds true. */
#define FAIL_IF(_cond)						\
	do {							\
		if ((_cond)) {					\
			return TEST_RESULT_FAIL;		\
		}						\
	} while (0)

/*
 * Send an SMC with the specified arguments.
 * Check that the values it returns match the expected ones. If not, write an
 * error message in the test report.
 */
static bool smc_check_eq(const smc_args *args, const smc_ret_values *expect)
{
	smc_ret_values ret = tftf_smc(args);

	if (memcmp(&ret, expect, sizeof(smc_ret_values)) == 0) {
		return true;
	} else {
		tftf_testcase_printf(
			"Got {0x%lx,0x%lx,0x%lx,0x%lx}, expected {0x%lx,0x%lx,0x%lx,0x%lx}.\n",
			ret.ret0, ret.ret1, ret.ret2, ret.ret3,
			expect->ret0, expect->ret1, expect->ret2, expect->ret3);
		return false;
	}
}

/* Exercise SMC32 calling convention with fast SMC calls. */
test_result_t smc32_fast(void)
{
	/* Valid Fast SMC32 using all 4 return values. */
	const smc_args args1 = { .fid = SMC_STD_SVC_UID };
	FAIL_IF(!smc_check_eq(&args1, &std_svc_uuid));

	/* Invalid Fast SMC32. */
	const smc_args args2 = {
		make_smc_fid(SMC_TYPE_FAST, SMC_32, OEN_ARM_START, INVALID_FN),
		0x11111111, 0x22222222, 0x33333333 };
	const smc_ret_values ret2
		= { SMC_UNKNOWN, 0x11111111, 0x22222222, 0x33333333 };
	FAIL_IF(!smc_check_eq(&args2, &ret2));

	/* Valid Fast SMC32 using 1 return value. */
	const smc_args args3
		= { SMC_PSCI_VERSION, 0x44444444, 0x55555555, 0x66666666 };
	const smc_ret_values ret3
		= { psci_version, 0x44444444, 0x55555555, 0x66666666 };
	FAIL_IF(!smc_check_eq(&args3, &ret3));

	return TEST_RESULT_SUCCESS;
}

/* Exercise SMC64 calling convention with yielding SMC calls. */
test_result_t smc64_yielding(void)
{
	/* Valid Fast SMC32 using all 4 return values. */
	const smc_args args1 = { .fid = SMC_STD_SVC_UID };
	FAIL_IF(!smc_check_eq(&args1, &std_svc_uuid));

	/* Invalid function number, SMC64 Yielding. */
	const smc_args args2 = {
		make_smc_fid(SMC_TYPE_STD, SMC_64, OEN_ARM_START, INVALID_FN),
		0x11111111, 0x22222222, 0x33333333 };
	const smc_ret_values ret2
		= { SMC_UNKNOWN, 0x11111111, 0x22222222, 0x33333333 };
	FAIL_IF(!smc_check_eq(&args2, &ret2));

	/*
	 * Valid[1] yielding SMC64 using 1 return value.
	 *
	 * [1] Valid from the point of view of the generic SMC handler if the
	 * TSPd is present. In this case, the SMC request gets passed to the
	 * TSPd handler code. The fact that it then gets rejected by the TSPd is
	 * irrelevant here, as we are not trying to test the TSPd nor the TSP.
	 *
	 * In other cases (i.e. AArch64 BL31 with no TSPd support or AArch32
	 * SP_MIN) this test should still fail in the same way, although it
	 * doesn't exercise the same code path in TF-A.
	 */
	const smc_args args3 = {
		make_smc_fid(SMC_TYPE_STD, SMC_64, OEN_TOS_START, INVALID_FN),
		0x44444444, 0x55555555, 0x66666666 };
	const smc_ret_values ret3
		= { SMC_UNKNOWN, 0x44444444, 0x55555555, 0x66666666 };
	FAIL_IF(!smc_check_eq(&args3, &ret3));

	return TEST_RESULT_SUCCESS;
}

#ifdef AARCH32
static test_result_t smc64_fast_caller32(void)
{
	/* Valid Fast SMC32 using all 4 return values. */
	smc_args args1 = { .fid = SMC_STD_SVC_UID };
	FAIL_IF(!smc_check_eq(&args1, &std_svc_uuid));

	/* Invalid SMC function number, Fast SMC64. */
	const smc_args args2 = {
		make_smc_fid(SMC_TYPE_FAST, SMC_64, OEN_ARM_START, INVALID_FN),
		0x11111111, 0x22222222, 0x33333333 };
	const smc_ret_values ret2
		= { SMC_UNKNOWN, 0x11111111, 0x22222222, 0x33333333 };
	FAIL_IF(!smc_check_eq(&args2, &ret2));

	/*
	 * Valid SMC function number, Fast SMC64. However, 32-bit callers are
	 * forbidden to use the SMC64 calling convention.
	 */
	const smc_args args3 = { SMC_PSCI_AFFINITY_INFO_AARCH64,
		0x44444444, 0x55555555, 0x66666666 };
	const smc_ret_values ret3
		= { SMC_UNKNOWN, 0x44444444, 0x55555555, 0x66666666 };
	FAIL_IF(!smc_check_eq(&args3, &ret3));

	return TEST_RESULT_SUCCESS;
}
#else
static test_result_t smc64_fast_caller64(void)
{
	/* Valid Fast SMC32 using all 4 return values. */
	smc_args args1 = { .fid = SMC_STD_SVC_UID };
	FAIL_IF(!smc_check_eq(&args1, &std_svc_uuid));

	/* Invalid function number, Fast SMC64. */
	const smc_args args2 = {
		make_smc_fid(SMC_TYPE_FAST, SMC_64, OEN_ARM_START, INVALID_FN),
		0x11111111, 0x22222222, 0x33333333 };
	const smc_ret_values ret2
		= { SMC_UNKNOWN, 0x11111111, 0x22222222, 0x33333333 };
	FAIL_IF(!smc_check_eq(&args2, &ret2));

	/* Valid Fast SMC64 using 1 return value. */
	const smc_args args3 = { SMC_PSCI_AFFINITY_INFO_AARCH64,
				 0x44444444, 0x55555555, 0x66666666 };
	const smc_ret_values ret3
		= { PSCI_E_INVALID_PARAMS, 0x44444444, 0x55555555, 0x66666666 };
	FAIL_IF(!smc_check_eq(&args3, &ret3));

	return TEST_RESULT_SUCCESS;
}
#endif /* AARCH32 */

/* Exercise SMC64 calling convention with fast SMC calls. */
test_result_t smc64_fast(void)
{
#ifdef AARCH32
	return smc64_fast_caller32();
#else
	return smc64_fast_caller64();
#endif
}

/* Exercise SMC32 calling convention with yielding SMC calls. */
test_result_t smc32_yielding(void)
{
	/* Valid Fast SMC32 using all 4 return values. */
	const smc_args args1 = { .fid = SMC_STD_SVC_UID };
	FAIL_IF(!smc_check_eq(&args1, &std_svc_uuid));

	/* Invalid function number, SMC32 Yielding. */
	const smc_args args2 = {
		make_smc_fid(SMC_TYPE_STD, SMC_32, OEN_ARM_START, INVALID_FN),
		0x11111111, 0x22222222, 0x33333333 };
	const smc_ret_values ret2
		= { SMC_UNKNOWN, 0x11111111, 0x22222222, 0x33333333 };
	FAIL_IF(!smc_check_eq(&args2, &ret2));

	/*
	 * Valid[1] yielding SMC32 using 1 return value.
	 *
	 * [1] Valid from the point of view of the generic SMC handler if a
	 * secure payload dispatcher handling this SMC range is present. In this
	 * case, the SMC request gets passed to the dispatcher handler code. The
	 * fact that it then gets rejected by the dispatcher is irrelevant here,
	 * as we are not trying to test the dispatcher nor the secure payload.
	 *
	 * In BL31 has no SPD support, this test should still fail in the same
	 * way, although it doesn't exercise the same code path in TF-A.
	 */
	const smc_args args3 = {
		make_smc_fid(SMC_TYPE_STD, SMC_32, OEN_TOS_START, INVALID_FN),
		0x44444444, 0x55555555, 0x66666666 };
	const smc_ret_values ret3
		= { SMC_UNKNOWN, 0x44444444, 0x55555555, 0x66666666 };
	FAIL_IF(!smc_check_eq(&args3, &ret3));

	return TEST_RESULT_SUCCESS;
}

