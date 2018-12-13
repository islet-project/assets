/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This test suite validates the execution state switch of a non-secure EL (from
 * AArch32 to AArch64, and vice versa) by issuing ARM SiP service SMC with
 * varying parameters. A cookie is shared between both states. A field in the
 * cookie is updated from the other state to signal that state switch did indeed
 * happen.
 *
 * Note that the suite is not AArch32-ready. All test cases will report as
 * skipped.
 */

#include <arch_helpers.h>
#include <debug.h>
#include <events.h>
#include <plat_topology.h>
#include <power_management.h>
#include <psci.h>
#include <tftf_lib.h>

/* Definitions from TF-A arm_sip_svc.h */
#define ARM_SIP_SVC_VERSION		0x8200ff03
#define ARM_SIP_SVC_EXE_STATE_SWITCH	0x82000020

/* State switch error codes from SiP service */
#define STATE_SW_E_PARAM		(-2)
#define STATE_SW_E_DENIED		(-3)

#define SIP_VERSION_OK			1

#define HI32(val)	(((u_register_t) (val)) >> 32)
#define LO32(val)	((uint32_t) (u_register_t) (val))

/* A cookie shared between states for information exchange */
typedef struct {
	uint32_t pc_hi;
	uint32_t pc_lo;
	uint64_t sp;
	uint32_t success;
} state_switch_cookie_t;

state_switch_cookie_t state_switch_cookie;
static event_t secondary_booted __unused;

/*
 * SiP service version check. Also a signal for test cases to execute or skip
 * altogether.
 */
static int sip_version_check __unused;

/* AArch32 instructions to switch state back to AArch64, stored as data */
extern void *state_switch_a32_entry;

extern int do_state_switch(void *);

/*
 * @Test_Aim@ Issue a system reset to initiate state switch SMC call that's part
 * of ARM SiP service. System reset is required because the state switch SMC
 * requires that no secondaries have been brought up since booting.
 */
test_result_t test_exec_state_switch_reset_before(void)
{
#ifdef AARCH64
	int version;
	smc_args sip_version_smc = { ARM_SIP_SVC_VERSION };
	smc_args reset = { SMC_PSCI_SYSTEM_RESET };
	smc_ret_values smc_ret;

#if NEW_TEST_SESSION
	/*
	 * This tests suite must start with a system reset. Following a reset,
	 * we expect TFTF to proceed with the rest of test cases. With
	 * NEW_TEST_SESSION set when built, TFTF will run this test case again
	 * after reset. Thus we'll continue resetting forever.
	 *
	 * If NEW_TEST_SESSION is set, skip this test case. sip_version_check
	 * won't be set to SIP_VERSION_OK, thereby skipping rest of test cases
	 * as well.
	 */
	tftf_testcase_printf("This suite needs TFTF built with NEW_TEST_SESSION=0\n");
	return TEST_RESULT_SKIPPED;
#endif

	/*
	 * Query system ARM SiP service version. State switch is available since
	 * version 0.2.
	 */
	smc_ret = tftf_smc(&sip_version_smc);
	if (((int) smc_ret.ret0) >= 0) {
		version = (smc_ret.ret0 << 8) | (smc_ret.ret1 & 0xff);
		if (version >= 0x02)
			sip_version_check = SIP_VERSION_OK;
	} else {
		tftf_testcase_printf("Test needs SiP service version 0.2 or later\n");
		return TEST_RESULT_SKIPPED;
	}

	/*
	 * This test will be continuously re-entered after reboot, until it
	 * returns success.
	 */
	if (tftf_is_rebooted())
		return TEST_RESULT_SUCCESS;

	tftf_testcase_printf("Issuing system reset before state switch\n");

	tftf_notify_reboot();
	tftf_smc(&reset);

	/* System reset is not expected to return */
	return TEST_RESULT_FAIL;
#else
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
#endif
}

/*
 * @Test_Aim@ Request execution state switch with invalid entry point. Expect
 * parameter error when switching from AArch64 to AArch32.
 */
test_result_t test_exec_state_switch_invalid_pc(void)
{
#ifdef AARCH64
	int ret;

	smc_args args = {
		.fid = ARM_SIP_SVC_EXE_STATE_SWITCH,
		.arg1 = (u_register_t) -1,
		.arg2 = LO32(&state_switch_a32_entry),
		.arg3 = HI32(&state_switch_cookie),
		.arg4 = LO32(&state_switch_cookie)
	};

	if (sip_version_check != SIP_VERSION_OK)
		return TEST_RESULT_SKIPPED;

	state_switch_cookie.success = 0;
	ret = do_state_switch(&args);
	if (state_switch_cookie.success || (ret != STATE_SW_E_PARAM))
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
#else
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
#endif
}

/*
 * @Test_Aim@ Request execution state switch with context_hi, and upper part of
 * context_lo set. Expect failure as they're not supposed to be set when
 * switching from AArch64 to AArch32.
 */
test_result_t test_exec_state_switch_invalid_ctx(void)
{
#ifdef AARCH64
	int ret;

	smc_args args = {
		.fid = ARM_SIP_SVC_EXE_STATE_SWITCH,
		.arg1 = HI32(&state_switch_a32_entry),
		.arg2 = LO32(&state_switch_a32_entry),
		.arg3 = -1,
		.arg4 = LO32(&state_switch_cookie)
	};

	if (sip_version_check != SIP_VERSION_OK)
		return TEST_RESULT_SKIPPED;

	state_switch_cookie.success = 0;
	ret = do_state_switch(&args);
	if (state_switch_cookie.success || (ret != STATE_SW_E_PARAM))
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
#else
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
#endif
}

/*
 * @Test_Aim@ Perform execution state switch, and back. We don't expect any
 * failures.
 */
test_result_t test_exec_state_switch_valid(void)
{
#ifdef AARCH64
	int ret;

	smc_args args = {
		.fid = ARM_SIP_SVC_EXE_STATE_SWITCH,
		.arg1 = HI32(&state_switch_a32_entry),
		.arg2 = LO32(&state_switch_a32_entry),
		.arg3 = HI32(&state_switch_cookie),
		.arg4 = LO32(&state_switch_cookie)
	};

	if (sip_version_check != SIP_VERSION_OK)
		return TEST_RESULT_SKIPPED;

	/* Make sure that we've a 32-bit PC to enter AArch32 */
	if (HI32(&state_switch_a32_entry)) {
		tftf_testcase_printf("AArch32 PC wider than 32 bits. Test skipped; needs re-link\n");
		return TEST_RESULT_SKIPPED;
	}

	/*
	 * Perform a state switch to 32 and back. Expect success field in the
	 * cookie to be set and return code zero,
	 */
	state_switch_cookie.success = 0;
	ret = do_state_switch(&args);
	if (!state_switch_cookie.success || (ret != 0))
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
#else
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
#endif
}

/*
 * Entry point for the secondary CPU. Send an event to the caller and returns
 * immediately.
 */
static inline test_result_t cpu_ping(void)
{
#ifdef AARCH64
	/* Tell the lead CPU that the calling CPU has entered the test */
	tftf_send_event(&secondary_booted);

	/*
	 * When returning from the function, the TFTF framework will power CPUs
	 * down, without this test needing to do anything
	 */
	return TEST_RESULT_SUCCESS;
#else
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
#endif
}

/*
 * @Test_Aim@ Power on any secondary and request a state switch. We expect the
 * request to be denied because a secondary had been brought up.
 */
test_result_t test_exec_state_switch_after_cpu_on(void)
{
#ifdef AARCH64
	u_register_t other_mpidr, my_mpidr;
	int ret;

	smc_args args = {
		.fid = ARM_SIP_SVC_EXE_STATE_SWITCH,
		.arg1 = HI32(&state_switch_a32_entry),
		.arg2 = LO32(&state_switch_a32_entry),
		.arg3 = HI32(&state_switch_cookie),
		.arg4 = LO32(&state_switch_cookie)
	};

	if (sip_version_check != SIP_VERSION_OK)
		return TEST_RESULT_SKIPPED;

	/* Make sure that we've a 32-bit PC to enter AArch32 */
	if (HI32(&state_switch_a32_entry)) {
		tftf_testcase_printf("AArch32 PC wider than 32 bits. Test skipped; needs re-link\n");
		return TEST_RESULT_SKIPPED;
	}

	tftf_init_event(&secondary_booted);

	/* Find a valid CPU to power on */
	my_mpidr = read_mpidr_el1() & MPID_MASK;
	other_mpidr = tftf_find_any_cpu_other_than(my_mpidr);
	if (other_mpidr == INVALID_MPID) {
		tftf_testcase_printf("Couldn't find a valid other CPU\n");
		return TEST_RESULT_FAIL;
	}

	/* Power on the other CPU */
	ret = tftf_cpu_on(other_mpidr, (uintptr_t) cpu_ping, 0);
	if (ret != PSCI_E_SUCCESS) {
		INFO("powering on %llx failed", (unsigned long long)
				other_mpidr);
		return TEST_RESULT_FAIL;
	}

	/* Wait for flag to proceed */
	tftf_wait_for_event(&secondary_booted);

	/*
	 * Request a state switch to 32 and back. Expect failure since we've
	 * powerd a secondary on.
	 */
	state_switch_cookie.success = 0;
	ret = do_state_switch(&args);
	if ((state_switch_cookie.success != 0) || (ret != STATE_SW_E_DENIED))
		return TEST_RESULT_FAIL;
	else
		return TEST_RESULT_SUCCESS;
#else
	tftf_testcase_printf("Test not ported to AArch32\n");
	return TEST_RESULT_SKIPPED;
#endif
}
