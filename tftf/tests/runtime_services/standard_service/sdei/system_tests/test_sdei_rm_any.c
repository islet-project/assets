/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <events.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <sdei.h>
#include <timer.h>
#include <tftf_lib.h>

/* This test makes sure RM_ANY can route SDEI events to all cores. */

extern sdei_handler_t sdei_rm_any_entrypoint;

typedef enum {
	CORE_STATUS_OFF = 0,
	CORE_STATUS_READY,
	CORE_STATUS_TRIGGERED
} core_status_et;

#define MPID_WAITING U(0xFFFFFFFF)

/*
 * These state variables are updated only by the lead CPU but are globals since
 * the lead CPU can change and also the event handler needs access to some of
 * them.
 */
static struct sdei_intr_ctx intr_ctx;
static volatile u_register_t mpid_lead;
static volatile int event;
static volatile unsigned int event_count;
static volatile unsigned int core_count;

/* These are shared variables that are written to by the event handler. */
static event_t exit_handler_event;
static volatile u_register_t mpid_last_handler;
static volatile core_status_et core_status[PLATFORM_CORE_COUNT];

/* Clean up on lead CPU after test completes or fails. */
static test_result_t cleanup(test_result_t result)
{
	long long ret;

	/*
	 * Sanity check that final event counter and core count match. If
	 * somehow a single event gets triggered on multiple cores these
	 * values will not match.
	 */
	if ((result == TEST_RESULT_SUCCESS) && (event_count != core_count)) {
		printf("Event count (%u) and core count (%u) mismatch!",
			event_count, core_count);
		result = TEST_RESULT_FAIL;
	}

	/* Unregister SDEI event. */
	ret = sdei_event_unregister(event);
	if (ret < 0) {
		printf("%u: %s failed (%lld)\n", __LINE__,
			"sdei_event_unregister", ret);
		result = TEST_RESULT_FAIL;
	}

	/* Unbind interrupt. */
	ret = sdei_interrupt_release(event, &intr_ctx);
	if (ret < 0) {
		printf("%u: %s failed (%lld)\n", __LINE__,
			"sdei_interrupt_release", ret);
		result = TEST_RESULT_FAIL;
	}
	return result;
}

/* Lead CPU selects an heir before it powers off. */
static test_result_t select_new_lead_cpu(void)
{
	/* Find a new lead CPU and update global. */
	for (unsigned int i = 0U; i < core_count; i++) {
		if (core_status[i] == CORE_STATUS_READY) {
			mpid_lead = tftf_plat_get_mpidr(i);
			return TEST_RESULT_SUCCESS;
		}
	}

	/* Should never get here. */
	return cleanup(TEST_RESULT_FAIL);
}

/* Lead CPU test manager. */
static test_result_t lead_cpu_manage_test(u_register_t mpid)
{
	/* Loop until handler runs on lead CPU. */
	while (mpid_last_handler != mpid_lead) {
		/* Set up next event to trigger in 50ms. */
		mpid_last_handler = MPID_WAITING;
		tftf_program_timer(50U);
		event_count++;

		/* Wait for event to set MPID and cancel timer if needed. */
		while (mpid_last_handler == MPID_WAITING) {
			/* Nothing to do here just wait. */
		}
		if (mpid_last_handler != mpid_lead) {
			tftf_cancel_timer();
			tftf_send_event(&exit_handler_event);
		}

		/* Check state of CPU events. */
		for (unsigned int i = 0U; i < core_count; i++) {
			if (core_status[i] != CORE_STATUS_TRIGGERED) {
				break;
			}
			if (i == (core_count - 1U)) {
				/* Done when all cores triggered. */
				return cleanup(TEST_RESULT_SUCCESS);
			}
		}
	}

	return select_new_lead_cpu();
}

/* All CPUs enter this function once test setup is done. */
static test_result_t test_loop(void)
{
	/* Get affinity information. */
	u_register_t mpid = read_mpidr_el1() & MPID_MASK;
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Unmask this CPU and mark it ready. */
	sdei_pe_unmask();
	core_status[core_pos] = CORE_STATUS_READY;

	/* Wait for status change or to be promoted to leader. */
	while (core_status[core_pos] != CORE_STATUS_TRIGGERED) {
		if (mpid_lead == mpid) {
			return lead_cpu_manage_test(mpid);
		}
	}

	return TEST_RESULT_SUCCESS;
}

/* Called from ASM SDEI handler function. */
void test_sdei_routing_any_handler(int ev, unsigned long long arg)
{
	/* Get affinity info. */
	u_register_t mpid = read_mpidr_el1() & MPID_MASK;

	/* Record event. */
	printf("Event handled on CPU%u\n", platform_get_core_pos(mpid));
	core_status[platform_get_core_pos(mpid)] = CORE_STATUS_TRIGGERED;
	mpid_last_handler = mpid;

	/*
	 * Timer must be cancelled by the lead CPU before returning from
	 * handler or the event will be triggered again.
	 */
	if (mpid == mpid_lead) {
		tftf_cancel_timer();
	} else {
		tftf_wait_for_event(&exit_handler_event);
	}
}

/* Lead CPU enters this function and sets up the test. */
test_result_t test_sdei_routing_any(void)
{
	u_register_t target_mpid;
	int cpu_node;
	long long ret;

	/* Set up test variables. */
	mpid_lead = read_mpidr_el1() & MPID_MASK;
	for (unsigned int i = 0U; i < PLATFORM_CORE_COUNT; i++) {
		core_status[i] = CORE_STATUS_OFF;
	}
	core_status[platform_get_core_pos(mpid_lead)] = CORE_STATUS_READY;
	event_count = 0U;
	mpid_last_handler = MPID_WAITING;
	tftf_init_event(&exit_handler_event);

	/* Make sure SDEI is supported before performing test. */
	ret = sdei_version();
	if (ret != MAKE_SDEI_VERSION(1U, 0U, 0U)) {
		printf("Unexpected SDEI version: 0x%llx\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	/* Initialize SDEI event to use TFTF timer as trigger. */
	event = sdei_interrupt_bind(tftf_get_timer_irq(), &intr_ctx);
	if (event < 0) {
		printf("%u: %s failed (%d)\n", __LINE__,
			"sdei_interrupt_bind", event);
		return TEST_RESULT_FAIL;
	}
	ret = sdei_event_register(event, sdei_rm_any_entrypoint, 0U,
			SDEI_REGF_RM_ANY, 0U);
	if (ret < 0) {
		printf("%u: %s failed (%lld)\n", __LINE__,
			"sdei_event_register", ret);
		return cleanup(TEST_RESULT_FAIL);
	}
	ret = sdei_event_enable(event);
	if (ret < 0) {
		printf("%u: %s failed (%lld)\n", __LINE__, "sdei_event_enable",
			ret);
		return cleanup(TEST_RESULT_FAIL);
	}

	/* Power on all CPUs and wait for them to be ready. */
	printf("Powering up CPUs.\n");
	core_count = 0U;
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (mpid_lead != target_mpid) {
			ret = tftf_cpu_on(target_mpid,
				(uintptr_t)test_loop, 0U);
			if (ret != PSCI_E_SUCCESS) {
				printf("CPU ON failed for 0x%llx\n",
					(unsigned long long)target_mpid);
				return cleanup(TEST_RESULT_FAIL);
			}
		}
		core_count++;
	}
	for (unsigned int i = 0U; i < core_count; i++) {
		if (core_status[i] != CORE_STATUS_READY) {
			i = 0U;
		}
	}

	/* Cores are powered up and in the loop, enter loop function. */
	printf("All CPUs ready, beginning test.\n");
	return test_loop();
}
