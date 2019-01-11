/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <events.h>
#include <irq.h>
#include <plat_topology.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <stdlib.h>
#include <tftf_lib.h>

static event_t cpu_ready[PLATFORM_CORE_COUNT];
static volatile unsigned int sgi_received[PLATFORM_CORE_COUNT];

static test_result_t (*psci_validate_test_function)(void);

/*
 * Sets sgi_received flag for indicating SGI is processed so that
 * test can exit in a clean state
 */
static int validate_pstate_sgi_handler(void *data)
{
	unsigned int core_pos;

	core_pos = platform_get_core_pos(read_mpidr_el1());
	sgi_received[core_pos] = 1;
	return 0;
}

/*
 * Gets the next possible composite state ID's combination and creates
 * a composite ID from 0 to max power levels in the system. It then calculates
 * whether the calculated composite ID is valid or invalid and validates EL3
 * firmware's return value.
 *
 * Returns:
 *         TEST_RESULT_SUCCESS : If PSCI return value is as expected
 *         TEST_RESULT_FAIL : If PSCI return value is not as expected
 */
static test_result_t validate_el3_pstate_parsing(void)
{
	unsigned int j;
	unsigned int test_suspend_type;
	unsigned int suspend_state_id;
	unsigned int power_level;
	int psci_ret;
	int expected_return_val;
	unsigned int power_state;
	unsigned int pstate_id_idx[PLAT_MAX_PWR_LEVEL + 1];

	INIT_PWR_LEVEL_INDEX(pstate_id_idx);

	for (j = 0; j <= PLAT_MAX_PWR_LEVEL; j++) {
		do {
			tftf_set_next_state_id_idx(j, pstate_id_idx);

			if (pstate_id_idx[0] == PWR_STATE_INIT_INDEX)
				break;

			expected_return_val = tftf_get_pstate_vars(&power_level,
								&test_suspend_type,
								&suspend_state_id,
								pstate_id_idx);
			power_state = tftf_make_psci_pstate(power_level,
							test_suspend_type,
							suspend_state_id);

			psci_ret = tftf_cpu_suspend(power_state);

			if (expected_return_val != psci_ret) {
				tftf_testcase_printf("Failed with values: "
							" psci_ret:%d"
							" expected_return_val:%d"
							" power_state:0x%x\n",
							psci_ret,
							expected_return_val,
							power_state);
				return TEST_RESULT_FAIL;
			}
		} while (1);
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Creates a composite state ID of a single valid local level above level
 * zero and tests the EL3 firmware's return value matches
 * PSCI_E_INVALID_PARAMS.
 *
 * For level 0, both local and composite power state are same. Hence, it's
 * skipped.
 *
 * Returns:
 *         TEST_RESULT_SUCCESS : If PSCI return value is as expected
 *         TEST_RESULT_FAIL : If PSCI return value is not as expected
 *         TEST_RESULT_SKIPPED : If PLAT_MAX_PWR_LEVEL is < 1
 */
static test_result_t valid_only_local_stateid(void)
{
	unsigned int power_state;
	int psci_ret;
	unsigned int pstate_id_idx[PLAT_MAX_PWR_LEVEL + 1];
	const plat_state_prop_t *local_level_state;
	unsigned int i;

	/* If only single power level is possible, SKIP the test */
	if (!PLAT_MAX_PWR_LEVEL) {
		tftf_testcase_printf("Platform has only a single valid local level\n");
		return TEST_RESULT_SKIPPED;
	}

	INIT_PWR_LEVEL_INDEX(pstate_id_idx);

	/*
	 * Start from power level 1, as local state for power level zero will
	 * be a valid composite id
	 */
	for (i = 1; i <= PLAT_MAX_PWR_LEVEL; i++) {
		do {

			INFO("Getting next local state:\n");
			tftf_set_next_local_state_id_idx(i, pstate_id_idx);

			if (pstate_id_idx[i] == PWR_STATE_INIT_INDEX)
				break;
			local_level_state = plat_get_state_prop(i) + pstate_id_idx[i];
			power_state = tftf_make_psci_pstate(i,
					local_level_state->is_pwrdown,
					local_level_state->state_ID << PLAT_LOCAL_PSTATE_WIDTH);

			psci_ret = tftf_cpu_suspend(power_state);

			if (psci_ret != PSCI_E_INVALID_PARAMS) {
				tftf_testcase_printf("Expected invalid params but got :"
					" psci_ret: %d"
					" power_state:0x%x\n",
					psci_ret,
					power_state);

				return TEST_RESULT_FAIL;
			}
		} while (pstate_id_idx[i] != PWR_STATE_INIT_INDEX);
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Create a composite state ID of invalid state ID's at all levels and
 * tests the EL3 firmware's return value matches PSCI_E_INVALID_PARAMS
 *
 * Returns:
 *         TEST_RESULT_SUCCESS : If PSCI return value is as expected
 *         TEST_RESULT_FAIL : If PSCI return value is not as expected
 */
static test_result_t completely_invalid_stateid(void)
{
	unsigned int state_id;
	int i;
	unsigned int power_state;
	int psci_ret;

	state_id = 0;

	/* Make stateID with all invalid ID's for all power levels */
	for (i = 0; i < PLAT_MAX_PWR_LEVEL; i++)
		state_id = state_id |
				((PLAT_PSCI_DUMMY_STATE_ID & ((1 << PLAT_LOCAL_PSTATE_WIDTH) - 1))
				 << (PLAT_LOCAL_PSTATE_WIDTH * i));

	power_state = tftf_make_psci_pstate(PLAT_MAX_PWR_LEVEL, PSTATE_TYPE_POWERDOWN, state_id);
	psci_ret = tftf_cpu_suspend(power_state);

	if (psci_ret != PSCI_E_INVALID_PARAMS) {
		tftf_testcase_printf("Expected invalid params but got : %d"
					" power_state:0x%x\n",
					psci_ret,
					power_state);

		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Creates a composite power state with invalid state type and tests
 * the EL3 firmware's return value matches PSCI_E_INVALID_PARAMS
 *
 * Returns:
 *         TEST_RESULT_SUCCESS : If PSCI return value is as expected
 *         TEST_RESULT_FAIL : If PSCI return value is not as expected
 */
static test_result_t invalid_state_type(void)
{
	unsigned int test_suspend_type;
	unsigned int suspend_state_id;
	unsigned int power_level;
	int psci_ret;
	int expected_return_val;
	unsigned int power_state;
	unsigned int pstate_id_idx[PLAT_MAX_PWR_LEVEL + 1];

	INIT_PWR_LEVEL_INDEX(pstate_id_idx);

	do {
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, pstate_id_idx);

		if (pstate_id_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		expected_return_val = tftf_get_pstate_vars(&power_level,
								&test_suspend_type,
								&suspend_state_id,
								pstate_id_idx);

		if (expected_return_val != PSCI_E_SUCCESS)
			continue;

		/* Reverse the suspend type */
		power_state = tftf_make_psci_pstate(power_level, !test_suspend_type, suspend_state_id);

		psci_ret = tftf_cpu_suspend(power_state);

		if (PSCI_E_INVALID_PARAMS != psci_ret) {
			tftf_testcase_printf("Failed with values:"
				" psci_ret:%d"
				" power_state:0x%x\n",
				psci_ret,
				power_state);
			return TEST_RESULT_FAIL;
		}
	} while (1);

	return TEST_RESULT_SUCCESS;
}

/*
 * Creates a composite power state with valid local state but invalid
 * power level and tests the EL3 firmware's return value matches
 * PSCI_E_INVALID_PARAMS
 *
 * Returns:
 *         TEST_RESULT_SUCCESS : If PSCI return value is as expected
 *         TEST_RESULT_FAIL : If PSCI return value is not as expected
 *         TEST_RESULT_SKIPPED : If EL3 firmware supports extended state ID
 */
static test_result_t invalid_power_level(void)
{
	unsigned int test_suspend_type;
	unsigned int suspend_state_id;
	unsigned int power_level;
	int psci_ret;
	int expected_return_val;
	unsigned int power_state;
	unsigned int pstate_id_idx[PLAT_MAX_PWR_LEVEL + 1];

	/* Skip the test if EL3 firmware code supports extended state ID */
	if (!tftf_is_psci_pstate_format_original())
		return TEST_RESULT_SKIPPED;

	INIT_PWR_LEVEL_INDEX(pstate_id_idx);

	do {
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, pstate_id_idx);

		if (pstate_id_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		expected_return_val = tftf_get_pstate_vars(&power_level,
					&test_suspend_type,
					&suspend_state_id,
					pstate_id_idx);

		if (expected_return_val != PSCI_E_SUCCESS)
			continue;

		/* Make a power state with invalid power level */
		power_state = tftf_make_psci_pstate(power_level + 1,
					test_suspend_type,
					suspend_state_id);

		psci_ret = tftf_cpu_suspend(power_state);

		if (PSCI_E_INVALID_PARAMS != psci_ret) {
			tftf_testcase_printf("Failed with values:"
					" psci_ret:%d"
					" power_state:0x%x\n",
					psci_ret,
					power_state);
			return TEST_RESULT_FAIL;
		}
	} while (1);

	return TEST_RESULT_SUCCESS;
}

/*
 * Creates a composite state ID of valid local state at some levels
 * and invalid state ID at others and tests the EL3 firmware's return
 * value matches PSCI_E_INVALID_PARAMS
 *
 * Returns:
 *         TEST_RESULT_SUCCESS : If PSCI return value is as expected
 *         TEST_RESULT_FAIL : If PSCI return value is not as expected
 *         TEST_RESULT_SKIPPED : If PLAT_MAX_PWR_LEVEL is < 1
 */
static test_result_t mixed_state_id(void)
{
	unsigned int test_suspend_type;
	unsigned int suspend_state_id;
	unsigned int power_level;
	int psci_ret;
	unsigned int power_state;
	unsigned int j;
	unsigned int pstate_id_idx[PLAT_MAX_PWR_LEVEL + 1];
	unsigned int invalid_id_set;

	/*
	 * Platform contains only one power level and hence we cann't have
	 * both valid and invalid local state
	 */
	if (!PLAT_MAX_PWR_LEVEL)
		return TEST_RESULT_SKIPPED;

	INIT_PWR_LEVEL_INDEX(pstate_id_idx);

	do {
		tftf_set_next_state_id_idx(PLAT_MAX_PWR_LEVEL, pstate_id_idx);

		if (pstate_id_idx[0] == PWR_STATE_INIT_INDEX)
			break;

		if (tftf_get_pstate_vars(&power_level,
					&test_suspend_type,
					&suspend_state_id,
					pstate_id_idx) != PSCI_E_SUCCESS)
			continue;

		invalid_id_set = 0;

		/*
		 * Generate a state ID with valid and invalid local state ID's at
		 * different levels
		 */
		for (j = 0; j <= power_level; j++) {
			/* Set index to invalid level for even power levels */
			if (rand() % 2) {
				suspend_state_id = suspend_state_id |
							((PLAT_PSCI_DUMMY_STATE_ID &
							((1 << PLAT_LOCAL_PSTATE_WIDTH) - 1))
							<< (PLAT_LOCAL_PSTATE_WIDTH * j));
				invalid_id_set = 1;
			}
		}

		/*
		 * Overwrite state ID for a random level if none of the
		 * levels are invalid
		 */
		if (!invalid_id_set) {
			j = rand() % (power_level + 1);
			suspend_state_id = suspend_state_id |
						((PLAT_PSCI_DUMMY_STATE_ID &
						((1 << PLAT_LOCAL_PSTATE_WIDTH) - 1))
						<< (PLAT_LOCAL_PSTATE_WIDTH * j));
		}

		power_state = tftf_make_psci_pstate(power_level, test_suspend_type, suspend_state_id);
		psci_ret = tftf_cpu_suspend(power_state);

		if (psci_ret != PSCI_E_INVALID_PARAMS) {
			tftf_testcase_printf("Failed with values: power_level: %d"
					" test_suspend_type: %d"
					" suspend_state_id:%d"
					" psci_ret:%d"
					" power_state:0x%x\n",
					power_level,
					test_suspend_type,
					suspend_state_id,
					psci_ret,
					power_state);
			return TEST_RESULT_FAIL;
		}
	} while (1);

	return TEST_RESULT_SUCCESS;
}


/*
 * This function contains common code for all test cases and runs the testcase
 * specific code.
 *
 * Returns the return value of test case specific code
 */
static test_result_t test_execute_test_function(void)
{
	test_result_t ret;
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());

	tftf_irq_register_handler(IRQ_NS_SGI_0, validate_pstate_sgi_handler);
	tftf_irq_enable(IRQ_NS_SGI_0, GIC_HIGHEST_NS_PRIORITY);

	/*
	 * Mask IRQ to prevent the interrupt handler being invoked
	 * and clearing the interrupt. A pending interrupt will cause this
	 * CPU to wake-up from suspend.
	 */
	disable_irq();

	/* Configure an SGI to wake-up from suspend  */
	tftf_send_sgi(IRQ_NS_SGI_0, core_pos);

	ret = (*psci_validate_test_function)();

	enable_irq();

	while (!sgi_received[core_pos])
		;

	tftf_irq_disable(IRQ_NS_SGI_0);
	tftf_irq_unregister_handler(IRQ_NS_SGI_0);

	return ret;
}

/*
 * Non-lead CPU entry point function for all PSCI PSTATE validation functions.
 *
 * Returns the return value of test case specific code
 */
static test_result_t test_non_lead_cpu_validate_ep(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());

	/*
	 * Tell the lead CPU that the calling CPU is ready to validate
	 * extended power state parsing
	 */
	tftf_send_event(&cpu_ready[core_pos]);

	return test_execute_test_function();
}

/*
 * Lead CPU entry point function for all PSCI PSTATE validation functions. It
 * powers on all secondaries and executes the test cases specific code.
 *
 * Returns the return value of test case specific code or SKIPPED in case
 * if it is unable to power on a core or EL3 firmware only supports NULL
 * stateID.
 */
static test_result_t test_lead_cpu_validate_ep(void)
{
	test_result_t ret;
	unsigned int core_pos;
	unsigned long lead_mpid;
	unsigned long target_mpid;
	unsigned long cpu_node;
	int i;

	if (tftf_is_psci_state_id_null()) {
		tftf_testcase_printf("EL3 firmware supports only NULL stateID\n");
		return TEST_RESULT_SKIPPED;
	}

	/* Initialise cpu_ready event variable */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++)
		tftf_init_event(&cpu_ready[i]);

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	/*
	 * Preparation step: Power on all cores.
	 */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU as it is already on */
		if (target_mpid == lead_mpid)
			continue;

		ret = tftf_cpu_on(target_mpid,
			(uintptr_t) test_non_lead_cpu_validate_ep,
				0);
		if (ret != PSCI_E_SUCCESS) {
			tftf_testcase_printf(
			"Failed to power on CPU 0x%x (%d)\n",
			(unsigned int)target_mpid, ret);

			return TEST_RESULT_SKIPPED;
		}
	}

	/* Wait for all non-lead CPUs to be ready */
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node);
		/* Skip lead CPU */
		if (target_mpid == lead_mpid)
			continue;

		core_pos = platform_get_core_pos(target_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	/* Call this to execute the test case specific code */
	return test_execute_test_function();
}

/*
 * Creates all possible valid local state ID's at all levels and tests
 * the EL3 firmware's return value matches the expected one.
 */
test_result_t test_psci_validate_pstate(void)
{
	psci_validate_test_function = &validate_el3_pstate_parsing;
	return test_lead_cpu_validate_ep();
}

/*
 * Creates a composite state ID of a single valid local level and
 * tests the EL3 firmware's return value matches the expected one.
 */
test_result_t test_psci_valid_local_pstate(void)
{
	psci_validate_test_function = &valid_only_local_stateid;
	return test_lead_cpu_validate_ep();
}

/*
 * Create a composite state ID of invalid state ID's at all levels
 * and tests the EL3 firmware's return value matches the expected
 * one.
 */
test_result_t test_psci_invalid_stateID(void)
{
	psci_validate_test_function = &completely_invalid_stateid;
	return test_lead_cpu_validate_ep();
}

/*
 * Creates a composite state ID of invalid state type and tests the
 * EL3 firmware's return value matches the expected one.
 */
test_result_t test_psci_invalid_state_type(void)
{
	psci_validate_test_function = &invalid_state_type;
	return test_lead_cpu_validate_ep();
}

/*
 * Creates a composite state ID of invalid power level in original
 * state format and tests the EL3 firmware's return value matches the
 * expected value.
 */
test_result_t test_psci_invalid_power_level(void)
{
	psci_validate_test_function = &invalid_power_level;
	return test_lead_cpu_validate_ep();
}

/*
 * Creates a composite state ID of valid local state at some levels
 * and invalid state ID at others and tests the EL3 firmware's return
 * value matches the expected value
 */
test_result_t test_psci_mixed_state_id(void)
{
	psci_validate_test_function = &mixed_state_id;
	return test_lead_cpu_validate_ep();
}
