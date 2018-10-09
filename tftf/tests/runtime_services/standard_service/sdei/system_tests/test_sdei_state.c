/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <plat_topology.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <sdei.h>
#include <stdint.h>
#include <tftf_lib.h>
#include <timer.h>

/*
 * These functions test the SDEI handler state transition as listed in section
 * in 6.1.2 of the specification.
 */

/* Aliases for SDEI handler states: 'R'unning, 'E'nabled, and re'G'istered */
#define r_		0
#define R_		(1u << 2)

#define e_		0
#define E_		(1u << 1)

#define g_		0
#define G_		(1u << 0)

/* All possible composite handler states */
#define reg_		(r_ | e_ | g_)
#define reG_		(r_ | e_ | G_)
#define rEg_		(r_ | E_ | g_)
#define rEG_		(r_ | E_ | G_)
#define Reg_		(R_ | e_ | g_)
#define ReG_		(R_ | e_ | G_)
#define REg_		(R_ | E_ | g_)
#define REG_		(R_ | E_ | G_)

#define is_running(st)		((st & R_) != 0)
#define is_enabled(st)		((st & E_) != 0)
#define is_registered(st)	((st & G_) != 0)

extern sdei_handler_t sdei_state_entrypoint;

static int handler_success;
static int32_t ev;

/* Dummy handler that won't be actually called. */
static int sdei_unreachable_handler(int ev, uint64_t arg)
{
	panic();
}

/* Test all failure transitions when handler state is handler unregistered */
static test_result_t hdlr_unregistered(void)
{
	int64_t status, ret;
	struct sdei_intr_ctx intr_ctx;

	ev = sdei_interrupt_bind(tftf_get_timer_irq(), &intr_ctx);
	if (ev < 0) {
		printf("%d: SDEI interrupt bind failed; ret=%d\n",
			__LINE__, ev);
		return TEST_RESULT_FAIL;
	}

	/* Query the state of handler. Expect it to be unregistered */
	status = sdei_event_status(ev);
	if (status != reg_) {
		printf("%d: Unexpected event status: 0x%llx != 0x%x\n",
			__LINE__, (unsigned long long) status, reg_);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_enable(ev);
	if (ret != -SMC_EDENY) {
		printf("%d: ENABLE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_disable(ev);
	if (ret != -SMC_EDENY) {
		printf("%d: DISABLE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_unregister(ev);
	if (ret != -SMC_EDENY) {
		printf("%d: UNREGISTER returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	/* Make sure we're operating on a shared interrupt */
	assert(tftf_get_timer_irq() >= 32);
	ret = sdei_event_routing_set(ev, SDEI_REGF_RM_PE);
	if (ret != -SMC_EDENY) {
		printf("%d: ROUTING_SET returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_context(0);
	if (ret != -SMC_EDENY) {
		printf("%d: EVENT_CONTEXT returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_complete(SDEI_EV_HANDLED);
	if (ret != -SMC_EDENY) {
		printf("%d: COMPLETE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_complete_and_resume(0);
	if (ret != -SMC_EDENY) {
		printf("%d: COMPLETE_AND_RESUME returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret < 0) {
		printf("%d: SDEI interrupt release failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

static test_result_t hdlr_registered(void)
{
	int64_t status, ret;
	struct sdei_intr_ctx intr_ctx;

	ev = sdei_interrupt_bind(tftf_get_timer_irq(), &intr_ctx);
	if (ev < 0) {
		printf("%d: SDEI interrupt bind failed; ret=%d\n",
			__LINE__,  ev);
		return TEST_RESULT_FAIL;
	}

	/* Register with dummy values. We aren't going to trigger the event */
	ret = sdei_event_register(ev, sdei_unreachable_handler, 0,
			SDEI_REGF_RM_PE, read_mpidr_el1());
	if (ret < 0) {
		printf("%d: SDEI interrupt register failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	/* Query the state of handler. Expect it to be registered */
	status = sdei_event_status(ev);
	if (status != reG_) {
		printf("%d: Unexpected event status: 0x%llx != 0x%x\n",
			__LINE__, (unsigned long long) status, reG_);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_register(ev, sdei_unreachable_handler, 0,
			SDEI_REGF_RM_PE, read_mpidr_el1());
	if (ret != -SMC_EDENY) {
		printf("%d: REGISTER returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret != -SMC_EDENY) {
		printf("%d: RELEASE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_context(0);
	if (ret != -SMC_EDENY) {
		printf("%d: EVENT_CONTEXT returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_complete(SDEI_EV_HANDLED);
	if (ret != -SMC_EDENY) {
		printf("%d: COMPLETE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_complete_and_resume(0);
	if (ret != -SMC_EDENY) {
		printf("%d: COMPLETE_AND_RESUME returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_unregister(ev);
	if (ret < 0) {
		printf("%d: SDEI unregister failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret < 0) {
		printf("%d: SDEI interrupt release failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

/* This is called from an SDEI handler */
static void running_handler(int ev, unsigned long long arg)
{
	int64_t ret, status;
	struct sdei_intr_ctx intr_ctx;

	/* Cancel timer to prevent further triggers */
	tftf_cancel_timer();

	handler_success = 0;

	/* Query the state of handler. Expect it to be registered */
	status = sdei_event_status(ev);
	if (status != REG_) {
		printf("%d: Unexpected event status: 0x%llx != 0x%x\n",
			__LINE__, (unsigned long long) status, REG_);
		return;
	}

	/* Call disable and check status again */
	ret = sdei_event_disable(ev);
	if (ret < 0) {
		printf("%d: DISABLE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	status = sdei_event_status(ev);
	if (status != ReG_) {
		printf("%d: Unexpected event status: 0x%llx != 0x%x\n",
			__LINE__, (unsigned long long) status, ReG_);
		return;
	}

	ret = sdei_event_register(ev, sdei_unreachable_handler, 0,
			SDEI_REGF_RM_PE, read_mpidr_el1());
	if (ret != -SMC_EDENY) {
		printf("%d: REGISTER returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret != -SMC_EDENY) {
		printf("%d: RELEASE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	ret = sdei_event_routing_set(ev, SDEI_REGF_RM_PE);
	if (ret != -SMC_EDENY) {
		printf("%d: ROUTING_SET returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	handler_success = 1;
}

test_result_t hdlr_registered_running(void)
{
	int64_t status, ret;
	struct sdei_intr_ctx intr_ctx;

	ev = sdei_interrupt_bind(tftf_get_timer_irq(), &intr_ctx);
	if (ev < 0) {
		printf("%d: SDEI interrupt bind failed; ret=%d\n",
			__LINE__, ev);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_register(ev, sdei_state_entrypoint,
			(uintptr_t) &running_handler, SDEI_REGF_RM_PE,
			read_mpidr_el1());
	if (ret < 0) {
		printf("%d: SDEI interrupt register failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	/* Query the state of handler. Expect it to be registered */
	status = sdei_event_status(ev);
	if (status != reG_) {
		printf("%d: Unexpected event status: 0x%llx != 0x%x\n",
			__LINE__, (unsigned long long) status, reG_);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_enable(ev);
	if (ret < 0) {
		printf("%d: SDEI event enable failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	sdei_pe_unmask();
	sdei_trigger_event();
	sdei_handler_done();

	ret = sdei_event_unregister(ev);
	if (ret < 0) {
		printf("%d: SDEI unregister failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret < 0) {
		printf("%d: SDEI interrupt release failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	if (handler_success == 0)
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
}

/* This is called from an SDEI handler */
static void ureg_running_handler(int ev, unsigned long long arg)
{
	int64_t ret, status;
	struct sdei_intr_ctx intr_ctx;

	/* Cancel timer to prevent further triggers */
	tftf_cancel_timer();

	handler_success = 0;

	/* Query the state of handler. Expect it to be registered */
	status = sdei_event_status(ev);
	if (!is_running(status)) {
		printf("%d: Handler reported as not running\n", __LINE__);
		return;
	}

	/*
	 * Unregister the event right away. Unregister while running must return
	 * pending error code.
	 */
	ret = sdei_event_unregister(ev);
	if (ret != -SMC_EPEND) {
		printf("%d: SDEI unregister failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	/*
	 * Query the state of handler. Expect it to be running-only now that
	 * we've unregistered.
	 */
	status = sdei_event_status(ev);
	if (status != Reg_) {
		printf("%d: Handler not reported as running-only\n", __LINE__);
		return;
	}

	ret = sdei_event_register(ev, sdei_unreachable_handler, 0,
			SDEI_REGF_RM_PE, read_mpidr_el1());
	if (ret != -SMC_EDENY) {
		printf("%d: REGISTER returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret != -SMC_EDENY) {
		printf("%d: RELEASE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	ret = sdei_event_enable(ev);
	if (ret != -SMC_EDENY) {
		printf("%d: ENABLE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	ret = sdei_event_disable(ev);
	if (ret != -SMC_EDENY) {
		printf("%d: DISABLE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	/* Unregister while running will return PEND */
	ret = sdei_event_unregister(ev);
	if (ret != -SMC_EPEND) {
		printf("%d: UNREGISTER returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	ret = sdei_event_routing_set(ev, SDEI_REGF_RM_PE);
	if (ret != -SMC_EDENY) {
		printf("%d: ROUTING_SET returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return;
	}

	handler_success = 1;
}

test_result_t hdlr_unregistered_running(void)
{
	int64_t status, ret;
	struct sdei_intr_ctx intr_ctx;

	ev = sdei_interrupt_bind(tftf_get_timer_irq(), &intr_ctx);
	if (ev < 0) {
		printf("%d: SDEI interrupt bind failed; ret=%d\n",
			__LINE__, ev);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_register(ev, sdei_state_entrypoint,
			(uintptr_t) &ureg_running_handler, SDEI_REGF_RM_PE,
			read_mpidr_el1());
	if (ret < 0) {
		printf("%d: SDEI interrupt register failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	/* Query the state of handler. Expect it to be registered */
	status = sdei_event_status(ev);
	if (status != reG_) {
		printf("%d: Unexpected event status: 0x%llx != 0x%x\n",
			__LINE__, (unsigned long long) status, reG_);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_enable(ev);
	if (ret < 0) {
		printf("%d: SDEI event enable failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	sdei_pe_unmask();
	sdei_trigger_event();
	sdei_handler_done();

	/*
	 * We've already unregistered the event within the handler, so this call
	 * must fail.
	 */
	ret = sdei_event_unregister(ev);
	if (ret != -SMC_EDENY) {
		printf("%d: SDEI unregister failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret < 0) {
		printf("%d: SDEI interrupt release failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	if (handler_success == 0)
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
}

test_result_t hdlr_enabled(void)
{
	int64_t status, ret;
	struct sdei_intr_ctx intr_ctx;

	ev = sdei_interrupt_bind(tftf_get_timer_irq(), &intr_ctx);
	if (ev < 0) {
		printf("%d: SDEI interrupt bind failed; ret=%d\n",
			__LINE__, ev);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_register(ev, sdei_state_entrypoint,
			(uintptr_t) &ureg_running_handler, SDEI_REGF_RM_PE,
			read_mpidr_el1());
	if (ret < 0) {
		printf("%d: SDEI interrupt register failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_enable(ev);
	if (ret < 0) {
		printf("%d: SDEI event enable failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	/*
	 * Query the state of handler. Expect it to be both registered and
	 * enabled.
	 */
	status = sdei_event_status(ev);
	if (status != rEG_) {
		printf("%d: Unexpected event status: 0x%llx != 0x%x\n",
			__LINE__, (unsigned long long) status, reG_);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_register(ev, sdei_unreachable_handler, 0,
			SDEI_REGF_RM_PE, read_mpidr_el1());
	if (ret != -SMC_EDENY) {
		printf("%d: REGISTER returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret != -SMC_EDENY) {
		printf("%d: RELEASE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_routing_set(ev, SDEI_REGF_RM_PE);
	if (ret != -SMC_EDENY) {
		printf("%d: ROUTING_SET returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_context(0);
	if (ret != -SMC_EDENY) {
		printf("%d: EVENT_CONTEXT returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_complete(SDEI_EV_HANDLED);
	if (ret != -SMC_EDENY) {
		printf("%d: COMPLETE returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_complete_and_resume(0);
	if (ret != -SMC_EDENY) {
		printf("%d: COMPLETE_AND_RESUME returned unexpected error code %lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_unregister(ev);
	if (ret < 0) {
		printf("%d: SDEI unregister failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_interrupt_release(ev, &intr_ctx);
	if (ret < 0) {
		printf("%d: SDEI interrupt release failed; ret=%lld\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}

static test_result_t iterate_state_machine(void)
{
	test_result_t ret;

	printf("%d: Cranking SDEI state machine on %llx\n",
		__LINE__, (unsigned long long) read_mpidr_el1());

	ret = hdlr_unregistered();
	if (ret != TEST_RESULT_SUCCESS) {
		printf("%d: State test hdlr_unregistered failed\n", __LINE__);
		return ret;
	}

	ret = hdlr_registered();
	if (ret != TEST_RESULT_SUCCESS) {
		printf("%d: State test hdlr_registered failed\n", __LINE__);
		return ret;
	}

	ret = hdlr_registered_running();
	if (ret != TEST_RESULT_SUCCESS) {
		printf("%d: State test hdlr_registered_running failed\n",
			__LINE__);
		return ret;
	}

	ret = hdlr_unregistered_running();
	if (ret != TEST_RESULT_SUCCESS) {
		printf("%d: State test hdlr_unregistered_running failed\n",
			__LINE__);
		return ret;
	}

	ret = hdlr_enabled();
	if (ret != TEST_RESULT_SUCCESS) {
		printf("%d: State test hdlr_enabled failed\n", __LINE__);
		return ret;
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Have all CPUs run through the SDEI state machine.
 */
test_result_t test_sdei_state(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node;
	int32_t aff_info __unused;
	int64_t ret;

	ret = sdei_version();
	if (ret != MAKE_SDEI_VERSION(1, 0, 0)) {
		printf("%d: Unexpected SDEI version: 0x%llx\n",
			__LINE__, (unsigned long long) ret);
		return TEST_RESULT_SKIPPED;
	}

	lead_mpid = read_mpidr_el1() & MPID_MASK;
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid) {
			/* Run on this CPU */
			if (iterate_state_machine() != TEST_RESULT_SUCCESS)
				return TEST_RESULT_FAIL;
		} else {
			/* Power on other CPU to run through SDEI state machine */
			ret = tftf_cpu_on(target_mpid,
					(uintptr_t) iterate_state_machine, 0);
			if (ret != PSCI_E_SUCCESS) {
				ERROR("CPU ON failed for 0x%llx\n",
						(unsigned long long) target_mpid);
				return TEST_RESULT_FAIL;
			}

			/* Wait for other CPU to power down */
			do {
				aff_info = tftf_psci_affinity_info(target_mpid,
						MPIDR_AFFLVL0);
			} while (aff_info != PSCI_STATE_OFF);
		}
	}

	return TEST_RESULT_SUCCESS;
}
