/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <events.h>
#include <nvm.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <tftf_lib.h>

#define PER_CPU_BUFFER_OFFSET 0x08

/* Events to specify activity to lead cpu */
static event_t cpu_ready[PLATFORM_CORE_COUNT];
static event_t test_done[PLATFORM_CORE_COUNT];

/* Used to make concurrent access to flash by all the cores */
static volatile int cpu_concurrent_write;

/*
 * @Test_Aim@ Test Non-Volatile Memory support
 *
 * Try reading/writing data from/to NVM to check that basic NVM support is
 * working as expected.
 */
test_result_t test_validation_nvm(void)
{
	STATUS status;
	unsigned test_value1 = 0x12345678;
	unsigned test_value2 = 0;

	/* Write a value in NVM */
	status = tftf_nvm_write(TFTF_STATE_OFFSET(testcase_buffer),
				&test_value1, sizeof(test_value1));
	if (status != STATUS_SUCCESS) {
		tftf_testcase_printf("tftf_nvm_write: error %d\n", status);
		return TEST_RESULT_FAIL;
	}

	/* Read it back from NVM */
	status = tftf_nvm_read(TFTF_STATE_OFFSET(testcase_buffer),
			&test_value2, sizeof(test_value2));
	if (status != STATUS_SUCCESS) {
		tftf_testcase_printf("tftf_nvm_read: error (%d)\n", status);
		return TEST_RESULT_FAIL;
	}

	/* Check that the 2 values match */
	if (test_value1 != test_value2) {
		tftf_testcase_printf("Values mismatch: %u != %u\n",
				test_value1, test_value2);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/* Odd CPU's write to flash and even CPU's read the flash */
static unsigned int access_flash_concurrent(unsigned int core_pos)
{
	unsigned int ret;
	unsigned int test_value;

	if (core_pos % 2) {
		ret = tftf_nvm_write(TFTF_STATE_OFFSET(testcase_buffer)
				+ core_pos * PER_CPU_BUFFER_OFFSET,
				&core_pos, sizeof(core_pos));
		if (ret != STATUS_SUCCESS) {
			tftf_testcase_printf("Write failed\n");
			return TEST_RESULT_FAIL;
		}
	} else {
		/* Dummy read */
		ret = tftf_nvm_read(TFTF_STATE_OFFSET(testcase_buffer),
				&test_value, sizeof(test_value));
		if (ret != STATUS_SUCCESS) {
			tftf_testcase_printf("Read failed\n");
			return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * @Test_Aim@ Test concurrent memory access to Non-Volatile Memory
 *
 * Try reading/writing data from multiple cores to NVM and verify the operations are
 * serialised and also the device does not crash.
 *
 */
static test_result_t test_validate_nvm_secondary(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);
	unsigned int ret;

	tftf_send_event(&cpu_ready[core_pos]);

	/* Wait until all cores are ready to access simultaneously */
	while (!cpu_concurrent_write)
		;

	ret = access_flash_concurrent(core_pos);

	tftf_send_event(&test_done[core_pos]);

	return ret;
}

/*
 * @Test_Aim@ Test serialisation of access by multiple CPU's
 *
 * Try reading/writing data to flash from all the CPU's with as much
 * concurrency as possible. Check the device does not hang and also
 * the update to flash happened as expected
 */
test_result_t test_validate_nvm_serialisation(void)
{
	unsigned int lead_mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int target_mpid, target_node;
	unsigned int core_pos;
	unsigned int lead_core_pos;
	unsigned int test_value;
	unsigned int ret;
	int rc;
	char init_buffer[TEST_BUFFER_SIZE] = {0};

	/* Initialise the scratch flash */
	ret = tftf_nvm_write(TFTF_STATE_OFFSET(testcase_buffer),
				&init_buffer,
				sizeof(init_buffer));
	if (ret != STATUS_SUCCESS) {
		tftf_testcase_printf("Write failed\n");
		return TEST_RESULT_FAIL;
	}

	/* Power on all the cores */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU as it is already on */
		if (target_mpid == lead_mpid)
			continue;
		rc = tftf_cpu_on(target_mpid,
				(uintptr_t) test_validate_nvm_secondary,
				0);
		if (rc != PSCI_E_SUCCESS) {
			tftf_testcase_printf("Failed to power on CPU 0x%x (%d)\n",
				target_mpid, rc);
			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for all non-lead CPU's to be ready */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU */
		if (target_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(target_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	lead_core_pos = platform_get_core_pos(read_mpidr_el1());

	/*
	 * Send event to all CPU's so that we can have as much concurrent
	 * access to flash as possible
	 */
	cpu_concurrent_write = 1;

	ret = access_flash_concurrent(lead_core_pos);

	if (ret != TEST_RESULT_SUCCESS)
		return ret;

	/* Wait for all non-lead CPU's to complete the test */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);
		/* Skip lead CPU */
		if (target_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(target_mpid);
		tftf_wait_for_event(&test_done[core_pos]);
	}

	/* Validate the results */
	for_each_cpu(target_node) {
		target_mpid = tftf_get_mpidr_from_node(target_node);

		core_pos = platform_get_core_pos(target_mpid);

		tftf_nvm_read(TFTF_STATE_OFFSET(testcase_buffer) +
				core_pos * PER_CPU_BUFFER_OFFSET,
				&test_value,
				sizeof(test_value));

		if ((core_pos % 2) && (test_value != core_pos)) {
			tftf_testcase_printf("Concurrent flash access test "
				"failed on cpu index: %d test_value:%d \n",
				core_pos, test_value);
			return TEST_RESULT_FAIL;
		} else if (((core_pos % 2) == 0) && (test_value != 0)) {
			tftf_testcase_printf("Concurrent flash access test "
				"failed on cpu index: %d test_value:%d \n",
				core_pos, test_value);
			 return TEST_RESULT_FAIL;
		}
	}

	return TEST_RESULT_SUCCESS;
}
