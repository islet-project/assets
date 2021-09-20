/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_features.h>
#include <arch_helpers.h>
#include <debug.h>
#include <events.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <sdei.h>
#include <test_helpers.h>
#include <tftf_lib.h>
#include <timer.h>

#ifdef __aarch64__

#define EV_COOKIE 0xDEADBEEF

extern sdei_handler_t sdei_check_pstate_entrypoint;

u_register_t daif;
u_register_t sp;
u_register_t pan;
u_register_t dit;

int sdei_check_pstate_handler(int ev, unsigned long long arg)
{
	printf("%s: handler fired\n", __func__);
	daif = read_daif();
	sp = read_spsel();
	if (is_armv8_1_pan_present())
		pan = read_pan();

	if (is_armv8_4_dit_present())
		dit = read_dit();

	assert(arg == EV_COOKIE);
	return 0;
}

static test_result_t sdei_event_check_pstate(void)
{
	long long ret;

	ret = sdei_event_register(0, sdei_check_pstate_entrypoint, EV_COOKIE,
		SDEI_REGF_RM_PE, read_mpidr_el1());
	if (ret < 0) {
		tftf_testcase_printf("SDEI event register failed: 0x%llx\n",
			ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_enable(0);
	if (ret < 0) {
		tftf_testcase_printf("SDEI event enable failed: 0x%llx\n", ret);
		goto err0;
	}

	ret = sdei_pe_unmask();
	if (ret < 0) {
		tftf_testcase_printf("SDEI pe unmask failed: 0x%llx\n", ret);
		goto err1;
	}

	/* Check the common bits are set correctly */
	ret = sdei_event_signal(read_mpidr_el1());
	if (ret < 0) {
		tftf_testcase_printf("SDEI event signal failed: 0x%llx\n", ret);
		goto err2;
	}
	sdei_handler_done();

	u_register_t all_interrupts_masked = 0x3c0;

	if (daif != all_interrupts_masked) {
		tftf_testcase_printf("Interrupts were not correctly masked " \
				     "during SDEI event signal\n" \
				     "Expected DAIF: 0x%lx, " \
				     "Actual DAIF: 0x%lx\n",
				     all_interrupts_masked, daif);
		ret = -1;
		goto err1;
	}

	u_register_t use_sp_elx = 0x1;

	if (sp != use_sp_elx) {
		tftf_testcase_printf("The SPSel PSTATE Bit was not set " \
				     "correctly during SDEI event signal\n" \
				     "Expected SPSel: 0x%lx, " \
				     "Actual SPSel: 0x%lx\n",
				     use_sp_elx, sp);
		ret = -1;
		goto err1;
	}

	if (is_armv8_1_pan_present()) {
		printf("PAN Enabled so testing PAN PSTATE bit\n");
		/*
		 * Check that when the SPAN bit is 0
		 * the PAN PSTATE bit is maintained
		 */

		/* When PAN bit is 0 */
		u_register_t expected_pan = 0;

		write_pan(expected_pan);
		ret = sdei_event_signal(read_mpidr_el1());
		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: " \
					     "0x%llx\n", ret);
			goto err2;
		}
		sdei_handler_done();
		if (pan != expected_pan) {
			tftf_testcase_printf("PAN PSTATE bit not maintained " \
					     "during SDEI event signal\n" \
					     "Expected PAN: 0x%lx, " \
					     "Actual PAN: 0x%lx\n",
					     expected_pan, pan);
			ret = -1;
			goto err1;
		}

		/* When PAN bit is 1 */
		expected_pan = PAN_BIT;
		write_pan(expected_pan);
		ret = sdei_event_signal(read_mpidr_el1());
		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: " \
					     "0x%llx\n", ret);
			goto err2;
		}
		sdei_handler_done();
		if (pan != expected_pan) {
			tftf_testcase_printf("PAN PSTATE bit not maintained " \
					     "during SDEI event signal\n" \
					     "Expected PAN: 0x%lx, " \
					     "Actual PAN: 0x%lx\n",
					     expected_pan, pan);
			ret = -1;
			goto err1;
		}

		/* Test that the SPAN condition is met */
		/* Set the SPAN bit */
		u_register_t old_sctlr = read_sctlr_el2();

		write_sctlr_el2(old_sctlr & ~SCTLR_SPAN_BIT);

		expected_pan = 0;
		/*
		 * Check that when the HCR_EL2.{E2H, TGE} != {1, 1}
		 * the PAN bit is maintained
		 */
		ret = sdei_event_signal(read_mpidr_el1());
		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: " \
					     "0x%llx\n", ret);
			goto err2;
		}
		sdei_handler_done();
		if (pan != expected_pan) {
			tftf_testcase_printf("PAN PSTATE bit not maintained " \
					     "during SDEI event signal " \
					     "when the SPAN bit is set and " \
					     "HCR_EL2.{E2H, TGE} != {1, 1}\n" \
					     "Expected PAN: 0x%lx, " \
					     "Actual PAN: 0x%lx\n",
					     expected_pan, pan);
			ret = -1;
			goto err1;
		}

		expected_pan = PAN_BIT;
		write_pan(expected_pan);
		ret = sdei_event_signal(read_mpidr_el1());
		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: " \
					     "0x%llx\n", ret);
			goto err2;
		}
		sdei_handler_done();
		if (pan != expected_pan) {
			tftf_testcase_printf("PAN PSTATE bit not maintained " \
					     "during SDEI event signal " \
					     "when the SPAN bit is set and " \
					     "HCR_EL2.{E2H, TGE} != {1, 1}\n" \
					     "Expected PAN: 0x%lx, " \
					     "Actual PAN: 0x%lx\n",
					     expected_pan, pan);
			ret = -1;
			goto err1;
		}

		/*
		 * Check that when the HCR_EL2.{E2H, TGE} = {1, 1}
		 * PAN bit is forced to 1
		 */
		/* Set E2H Bit */
		u_register_t old_hcr_el2 = read_hcr_el2();

		write_hcr_el2(old_hcr_el2 | HCR_E2H_BIT);

		ret = sdei_event_signal(read_mpidr_el1());
		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: " \
					     "0x%llx\n", ret);
			goto err2;
		}
		sdei_handler_done();
		if (pan != PAN_BIT) {
			tftf_testcase_printf("PAN PSTATE bit was not forced " \
					     "to 1 during SDEI event signal " \
					     "when the SPAN bit is set and " \
					     "HCR_EL2.{E2H, TGE} = {1, 1}\n");
			ret = -1;
			goto err1;
		}

		/*
		 * Set the SCTLR and HCR_EL2 registers back to their old values
		 */
		write_sctlr_el2(old_sctlr);
		write_hcr_el2(old_hcr_el2);
	}

	/* Check that the DIT PSTATE bit is maintained during event signal */
	if (is_armv8_4_dit_present()) {
		printf("DIT Enabled so testing DIT PSTATE bit\n");
		/* When DIT bit is 0 */
		u_register_t expected_dit = 0;

		write_dit(expected_dit);
		ret = sdei_event_signal(read_mpidr_el1());

		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: " \
					     "0x%llx\n", ret);
			goto err2;
		}
		sdei_handler_done();
		if (dit != expected_dit) {
			tftf_testcase_printf("DIT PSTATE bit not maintained " \
					     "during SDEI event signal\n" \
					     "Expected DIT: 0x%lx, " \
					     "Actual DIT: 0x%lx\n",
					     expected_dit, dit);
			ret = -1;
			goto err1;
		}

		/* When dit bit is 1 */
		expected_dit = DIT_BIT;
		write_dit(expected_dit);
		ret = sdei_event_signal(read_mpidr_el1());
		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: " \
					     "0x%llx\n", ret);
			goto err2;
		}
		sdei_handler_done();
		if (dit != expected_dit) {
			tftf_testcase_printf("DIT PSTATE bit not maintained " \
					     "during SDEI event signal\n" \
					     "Expected DIT: 0x%lx, " \
					     "Actual DIT: 0x%lx\n",
					     expected_dit, dit);
			ret = -1;
			goto err1;
		}
	}

err2:
	sdei_pe_mask();
err1:
	sdei_event_disable(0);
err0:
	sdei_event_unregister(0);

	if (ret < 0)
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
}
#endif /* __aarch64__ */

/* Each core signals itself using SDEI event signalling. */
test_result_t test_sdei_event_check_pstate(void)
{
	SKIP_TEST_IF_AARCH32();
#ifdef __aarch64__
	long long ret;

	ret = sdei_version();
	if (ret != MAKE_SDEI_VERSION(1, 0, 0)) {
		tftf_testcase_printf("Unexpected SDEI version: 0x%llx\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	disable_irq();
	/* We only need to run these tests on the main CPU */
	if (sdei_event_check_pstate() != TEST_RESULT_SUCCESS) {
		ret = -1;
		goto err0;
	}

err0:
	enable_irq();
	if (ret < 0)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
#endif /* __aarch64__ */
}
