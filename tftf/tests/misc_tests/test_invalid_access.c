/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <plat/common/platform.h>

#include <arch.h>
#include <arch_helpers.h>
#include <arch_features.h>
#include <debug.h>
#ifdef __aarch64__
#include <sync.h>
#endif
#include <test_helpers.h>
#include <lib/aarch64/arch_features.h>
#include <runtime_services/realm_payload/realm_payload_test.h>
#include <tftf_lib.h>
#include <platform_def.h>

/*
 * Using "__aarch64__" here looks weird but its unavoidable because of following reason
 * This test is part of standard test which runs on all platforms but pre-requisite
 * to run this test (custom sync exception handler) is only implemented for aarch64.
 * TODO:  Write a framework so that tests kept in standard list can be selectively
 * run on a given architecture
 */
#ifdef __aarch64__

static volatile bool sync_exception_triggered;
static volatile bool data_abort_triggered;

static __aligned(PAGE_SIZE) uint64_t share_page[PAGE_SIZE / sizeof(uint64_t)];

static bool data_abort_handler(void)
{
	uint64_t esr_elx = IS_IN_EL2() ? read_esr_el2() : read_esr_el1();
	unsigned int rme_supported = get_armv9_2_feat_rme_support();

	sync_exception_triggered = true;

	VERBOSE("%s esr_elx %llx\n", __func__, esr_elx);

	if (EC_BITS(esr_elx) == EC_DABORT_CUR_EL) {
		if (rme_supported == 0) {
			/* Synchronous external data abort triggered by trustzone controller */
			if ((ISS_BITS(esr_elx) & ISS_DFSC_MASK) == DFSC_EXT_DABORT) {
				VERBOSE("%s TZC Data Abort caught\n", __func__);
				data_abort_triggered = true;
				return true;
			}
		} else {
			/* Synchronous data abort triggered by Granule protection */
			if ((ISS_BITS(esr_elx) & ISS_DFSC_MASK) == DFSC_GPF_DABORT) {
				VERBOSE("%s GPF Data Abort caught\n", __func__);
				data_abort_triggered = true;
				return true;
			}
		}
	}

	return false;
}

test_result_t access_el3_memory_from_ns(void)
{
	const uintptr_t test_address = EL3_MEMORY_ACCESS_ADDR;

	VERBOSE("Attempt to access el3 memory (0x%lx)\n", test_address);

	data_abort_triggered = false;
	register_custom_sync_exception_handler(data_abort_handler);
	dsbsy();

	*((volatile uint64_t *)test_address);

	dsbsy();
	unregister_custom_sync_exception_handler();

	if (sync_exception_triggered == false) {
		tftf_testcase_printf("No sync exception while accessing (0x%lx)\n", test_address);
		return TEST_RESULT_SKIPPED;
	}

	if (data_abort_triggered == false) {
		tftf_testcase_printf("Sync exception is not data abort\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * @Test_Aim@ Check a realm region cannot be accessed from normal world.
 *
 * This test delegates a TFTF allocated buffer to Realm. It then attempts
 * a read access to the region from normal world. This results in the PE
 * triggering a GPF caught by a custom synchronous abort handler.
 *
 */
test_result_t rl_memory_cannot_be_accessed_in_ns(void)
{
	test_result_t result = TEST_RESULT_FAIL;
	u_register_t retmm;

	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	sync_exception_triggered = false;
	data_abort_triggered = false;
	register_custom_sync_exception_handler(data_abort_handler);

	/* First read access to the test region must not fail. */
	*((volatile uint64_t *)share_page);

	if ((sync_exception_triggered != false) ||
	    (data_abort_triggered != false)) {
		goto out_unregister;
	}

	/* Delegate the shared page to Realm. */
	retmm = realm_granule_delegate((u_register_t)&share_page);
	if (retmm != 0UL) {
		ERROR("Granule delegate failed!\n");
		goto out_unregister;
	}

	/* This access shall trigger a GPF. */
	*((volatile uint64_t *)share_page);

	if ((sync_exception_triggered != true) ||
	    (data_abort_triggered != true)) {
		goto out_undelegate;
	}

	result = TEST_RESULT_SUCCESS;

out_undelegate:
	/* Undelegate the shared page. */
	retmm = realm_granule_undelegate((u_register_t)&share_page);
	if (retmm != 0UL) {
		ERROR("Granule undelegate failed!\n");
	}

out_unregister:
	unregister_custom_sync_exception_handler();

	return result;
}

#else

test_result_t access_el3_memory_from_ns(void)
{
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
}

test_result_t rl_memory_cannot_be_accessed_in_ns(void)
{
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
}

#endif /* __aarch64__ */
