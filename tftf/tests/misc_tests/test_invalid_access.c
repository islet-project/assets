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
#include <host_realm_helper.h>
#include <lib/aarch64/arch_features.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <xlat_tables_v2.h>
#include <platform_def.h>
#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>

/*
 * Using "__aarch64__" here looks weird but its unavoidable because of following reason
 * This test is part of standard test which runs on all platforms but pre-requisite
 * to run this test (custom sync exception handler) is only implemented for aarch64.
 * TODO:  Write a framework so that tests kept in standard list can be selectively
 * run on a given architecture
 */
#ifdef __aarch64__

#define SENDER HYP_ID
#define RECEIVER SP_ID(1)

static volatile bool sync_exception_triggered;
static volatile bool data_abort_triggered;
static const struct ffa_uuid expected_sp_uuids[] = {
		{PRIMARY_UUID}, {SECONDARY_UUID}, {TERTIARY_UUID}
};

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

test_result_t el3_memory_cannot_be_accessed_in_ns(void)
{
	const uintptr_t test_address = EL3_MEMORY_ACCESS_ADDR;

	VERBOSE("Attempt to access el3 memory (0x%lx)\n", test_address);

	sync_exception_triggered = false;
	data_abort_triggered = false;

	int rc = mmap_add_dynamic_region(test_address, test_address, PAGE_SIZE,
					MT_MEMORY | MT_RW | MT_NS);
	if (rc != 0) {
		tftf_testcase_printf("%d: mmap_add_dynamic_region() = %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	register_custom_sync_exception_handler(data_abort_handler);
	*((volatile uint64_t *)test_address);
	unregister_custom_sync_exception_handler();

	rc = mmap_remove_dynamic_region(test_address, PAGE_SIZE);
	if (rc != 0) {
		tftf_testcase_printf("%d: mmap_remove_dynamic_region() = %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

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
	retmm = rmi_granule_delegate((u_register_t)&share_page);
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
	retmm = rmi_granule_undelegate((u_register_t)&share_page);
	if (retmm != 0UL) {
		ERROR("Granule undelegate failed!\n");
	}

out_unregister:
	unregister_custom_sync_exception_handler();

	return result;
}

/**
 * @Test_Aim@ Check a secure region cannot be accessed from normal world.
 *
 * Following test intends to run on RME enabled platforms when EL3
 * is Root world. In a non RME platform, EL3 is secure.
 * Access to secure memory from NS world is already covered
 * by el3_memory_cannot_be_accessed_in_ns.
 */
test_result_t s_memory_cannot_be_accessed_in_ns(void)
{
	const uintptr_t test_address = SECURE_MEMORY_ACCESS_ADDR;

	/* skipp non RME platforms */
	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	VERBOSE("Attempt to access secure memory (0x%lx)\n", test_address);

	data_abort_triggered = false;
	sync_exception_triggered = false;
	register_custom_sync_exception_handler(data_abort_handler);
	dsbsy();

	int rc = mmap_add_dynamic_region(test_address, test_address, PAGE_SIZE,
					MT_MEMORY | MT_RW | MT_NS);

	if (rc != 0) {
		tftf_testcase_printf("%d: mmap_add_dynamic_region() = %d\n", __LINE__, rc);
		return TEST_RESULT_FAIL;
	}

	*((volatile uint64_t *)test_address);

	mmap_remove_dynamic_region(test_address, PAGE_SIZE);

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

static test_result_t memory_cannot_be_accessed_in_rl(u_register_t params)
{
	u_register_t retrmm;
	static char rd[GRANULE_SIZE] __aligned(GRANULE_SIZE);

	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	retrmm = rmi_version();

	VERBOSE("RMM version is: %lu.%lu\n",
			RMI_ABI_VERSION_GET_MAJOR(retrmm),
			RMI_ABI_VERSION_GET_MINOR(retrmm));

	/*
	 * TODO: Remove this once SMC_RMM_REALM_CREATE is implemented in TRP
	 * For the moment skip the test if RMM is TRP, TRP version is always null.
	 */
	if (retrmm == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	retrmm = rmi_granule_delegate((u_register_t)&rd[0]);
	if (retrmm != 0UL) {
		ERROR("Delegate operation returns fail, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}

	/* Create a realm using a parameter in a secure physical address space should fail. */
	retrmm = rmi_realm_create((u_register_t)&rd[0], params);
	if (retrmm == 0UL) {
		ERROR("Realm create operation should fail, %lx\n", retrmm);
		retrmm = rmi_realm_destroy((u_register_t)&rd[0]);
		if (retrmm != 0UL) {
			ERROR("Realm destroy operation returns fail, %lx\n", retrmm);
			return TEST_RESULT_FAIL;
		}
		return TEST_RESULT_FAIL;
	} else if (retrmm != RMM_STATUS_ERROR_INPUT) {
		ERROR("Realm create operation should fail with code:%ld retrmm:%ld\n",
		RMM_STATUS_ERROR_INPUT, retrmm);
		return TEST_RESULT_FAIL;
	}

	retrmm = rmi_granule_undelegate((u_register_t)&rd[0]);
	if (retrmm != 0UL) {
		INFO("Undelegate operation returns fail, %lx\n", retrmm);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * @Test_Aim@ Check a root region cannot be accessed from a secure partition.
 *
 * This change adds TFTF and cactus test to permit checking a root region
 * cannot be accessed from secure world.
 * A hardcoded address marked Root in the GPT is shared to a secure
 * partition. The SP retrieves the region from the SPM, maps it and
 * attempts a read access to the region. It is expected to trigger a GPF
 * data abort on the PE caught by a custom exception handler.
 *
 */
test_result_t rt_memory_cannot_be_accessed_in_s(void)
{
	const uintptr_t test_address = EL3_MEMORY_ACCESS_ADDR;
	struct ffa_memory_region_constituent constituents[] = {
		{
			(void *)test_address, 1, 0
		}
	};
	const uint32_t constituents_count = sizeof(constituents) /
		sizeof(struct ffa_memory_region_constituent);
	ffa_memory_handle_t handle;
	struct mailbox_buffers mb;
	struct ffa_value ret;

	if (get_armv9_2_feat_rme_support() == 0U) {
		return TEST_RESULT_SKIPPED;
	}

	INIT_TFTF_MAILBOX(mb);

	CHECK_SPMC_TESTING_SETUP(1, 1, expected_sp_uuids);

	GET_TFTF_MAILBOX(mb);

	handle = memory_init_and_send((struct ffa_memory_region *)mb.send,
					PAGE_SIZE, SENDER, RECEIVER,
					constituents, constituents_count,
					FFA_MEM_SHARE_SMC32, &ret);

	if (handle == FFA_MEMORY_HANDLE_INVALID) {
		return TEST_RESULT_FAIL;
	}

	VERBOSE("TFTF - Handle: %llx Address: %p\n",
		handle, constituents[0].address);

	/* Retrieve the shared page and attempt accessing it. */
	ret = cactus_mem_send_cmd(SENDER, RECEIVER, FFA_MEM_SHARE_SMC32,
				  handle, 0, true, 1);

	if (is_ffa_call_error(ffa_mem_reclaim(handle, 0))) {
		ERROR("Memory reclaim failed!\n");
		return TEST_RESULT_FAIL;
	}

	/*
	 * Expect success response with value 1 hinting an exception
	 * triggered while the SP accessed the region.
	 */
	if (!(cactus_get_response(ret) == CACTUS_SUCCESS &&
	      cactus_error_code(ret) == 1)) {
		ERROR("Exceptions test failed!\n");
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

test_result_t s_memory_cannot_be_accessed_in_rl(void)
{
	u_register_t params = (u_register_t)SECURE_MEMORY_ACCESS_ADDR;
	return memory_cannot_be_accessed_in_rl(params);
}

test_result_t rt_memory_cannot_be_accessed_in_rl(void)
{
	u_register_t params = (u_register_t)EL3_MEMORY_ACCESS_ADDR;
	return memory_cannot_be_accessed_in_rl(params);
}

#else

test_result_t el3_memory_cannot_be_accessed_in_ns(void)
{
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
}

test_result_t rl_memory_cannot_be_accessed_in_ns(void)
{
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
}

test_result_t s_memory_cannot_be_accessed_in_ns(void)
{
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
}

test_result_t s_memory_cannot_be_accessed_in_rl(void)
{
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
}

test_result_t rt_memory_cannot_be_accessed_in_rl(void)
{
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
}

#endif /* __aarch64__ */
