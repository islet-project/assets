/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <smccc.h>

#include <arch_helpers.h>
#include <cactus_test_cmds.h>
#include <ffa_endpoints.h>
#include <ffa_svc.h>
#include <platform.h>
#include <spm_common.h>
#include <test_helpers.h>

/**
 * Helper to create bitmap for NWd VMs.
 */
static bool notifications_bitmap_create(ffa_id_t vm_id,
					ffa_vcpu_count_t vcpu_count)
{
	VERBOSE("Creating bitmap for VM %x; cpu count: %u.\n",
		vm_id, vcpu_count);
	smc_ret_values ret = ffa_notification_bitmap_create(vm_id, vcpu_count);

	return !is_ffa_call_error(ret);
}

/**
 * Helper to destroy bitmap for NWd VMs.
 */
static bool notifications_bitmap_destroy(ffa_id_t vm_id)
{
	VERBOSE("Destroying bitmap of VM %x.\n", vm_id);
	smc_ret_values ret = ffa_notification_bitmap_destroy(vm_id);

	return !is_ffa_call_error(ret);
}

/**
 * Test notifications bitmap create and destroy interfaces.
 */
test_result_t test_ffa_notifications_bitmap_create_destroy(void)
{
	const ffa_id_t vm_id = HYP_ID + 1;

	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 1);

	if (check_spmc_execution_level()) {
		VERBOSE("OPTEE as SPMC at S-EL1. Skipping test!\n");
		return TEST_RESULT_SKIPPED;
	}

	if (!notifications_bitmap_create(vm_id, PLATFORM_CORE_COUNT)) {
		return TEST_RESULT_FAIL;
	}

	if (!notifications_bitmap_destroy(vm_id)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * Test notifications bitmap destroy in a case the bitmap hasn't been created.
 */
test_result_t test_ffa_notifications_destroy_not_created(void)
{
	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 1);

	if (check_spmc_execution_level()) {
		VERBOSE("OPTEE as SPMC at S-EL1. Skipping test!\n");
		return TEST_RESULT_SKIPPED;
	}

	smc_ret_values ret = ffa_notification_bitmap_destroy(HYP_ID + 1);

	if (!is_expected_ffa_error(ret, FFA_ERROR_DENIED)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/**
 * Test attempt to create notifications bitmap for NWd VM if it had been
 * already created.
 */
test_result_t test_ffa_notifications_create_after_create(void)
{
	smc_ret_values ret;
	const ffa_id_t vm_id = HYP_ID + 2;

	SKIP_TEST_IF_FFA_VERSION_LESS_THAN(1, 1);

	if (check_spmc_execution_level()) {
		VERBOSE("OPTEE as SPMC at S-EL1. Skipping test!\n");
		return TEST_RESULT_SKIPPED;
	}

	/* First successfully create a notifications bitmap */
	if (!notifications_bitmap_create(vm_id, 1)) {
		return TEST_RESULT_FAIL;
	}

	/* Attempt to do the same to the same VM. */
	ret = ffa_notification_bitmap_create(vm_id, 1);

	if (!is_expected_ffa_error(ret, FFA_ERROR_DENIED)) {
		return TEST_RESULT_FAIL;
	}

	/* Destroy to not affect other tests */
	if (!notifications_bitmap_destroy(vm_id)) {
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
