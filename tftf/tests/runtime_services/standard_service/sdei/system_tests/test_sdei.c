/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <drivers/arm/private_timer.h>
#include <events.h>
#include <plat_topology.h>
#include <platform.h>
#include <power_management.h>
#include <sdei.h>
#include <tftf_lib.h>
#include <timer.h>

#define EV_COOKIE 0xDEADBEEF
#define TIMER_TIMEO_MS 10

extern sdei_handler_t sdei_entrypoint;
extern sdei_handler_t sdei_entrypoint_resume;

/*
 * the bound event number as returned from sdei_interrupt_bind(), passed
 * to the per-cpu SDEI test function
 */
static int bound_ev;
/* true if the test is using a private interrupt source, false otherwise. */
static int private_interrupt;

static spinlock_t cpu_count_lock;
static volatile int cpu_count;
static volatile int participating_cpu_count;

/* Helper function to wait for CPUs participating in the test. */
static void wait_for_participating_cpus(void)
{
	assert(participating_cpu_count <= PLATFORM_CORE_COUNT);

	spin_lock(&cpu_count_lock);
	cpu_count++;
	spin_unlock(&cpu_count_lock);

	assert(cpu_count <= PLATFORM_CORE_COUNT);

	while (cpu_count != participating_cpu_count)
		continue;
}

void sdei_trigger_event(void)
{
	printf("%s: triggering SDEI event\n", __func__);
	if (private_interrupt)
		private_timer_start(TIMER_TIMEO_MS);
	else
		tftf_program_timer(TIMER_TIMEO_MS);
}

static test_result_t sdei_event(void)
{
	long long ret;

	wait_for_participating_cpus();

	printf("%s: mpidr = 0x%llx\n", __func__,
		(unsigned long long)read_mpidr_el1());

	ret = sdei_event_register(bound_ev, sdei_entrypoint_resume, EV_COOKIE,
		SDEI_REGF_RM_PE, read_mpidr_el1());
	if (ret < 0) {
		tftf_testcase_printf("SDEI event register failed: 0x%llx\n",
			ret);
		return TEST_RESULT_FAIL;
	}

	ret = sdei_event_enable(bound_ev);
	if (ret < 0) {
		tftf_testcase_printf("SDEI event enable failed: 0x%llx\n", ret);
		goto err0;
	}

	ret = sdei_pe_unmask();
	if (ret < 0) {
		tftf_testcase_printf("SDEI pe unmask failed: 0x%llx\n", ret);
		goto err1;
	}

	sdei_trigger_event();

	sdei_handler_done();

	sdei_pe_mask();

err1:
	sdei_event_disable(bound_ev);
err0:
	sdei_event_unregister(bound_ev);

	if (ret < 0)
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
}

int sdei_event_handler(int ev, unsigned long long arg)
{
	printf("%s: handler fired\n", __func__);
	assert(arg == EV_COOKIE);
	if (private_interrupt)
		private_timer_stop();
	else
		tftf_cancel_timer();
	return 0;
}

/* Handle an SDEI event on all cores in sequence. */
test_result_t test_sdei_event_serial(void)
{
	struct sdei_intr_ctx intr_ctx;
	u_register_t lead_mpid, target_mpid;
	int cpu_node;
	long long ret;

	lead_mpid = read_mpidr_el1() & MPID_MASK;
	participating_cpu_count = 1;
	init_spinlock(&cpu_count_lock);
	cpu_count = 0;

	ret = sdei_version();
	if (ret != MAKE_SDEI_VERSION(1, 0, 0)) {
		tftf_testcase_printf("Unexpected SDEI version: 0x%llx\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	disable_irq();
	bound_ev = sdei_interrupt_bind(tftf_get_timer_irq(), &intr_ctx);
	if (bound_ev < 0) {
		tftf_testcase_printf("SDEI interrupt bind failed: %x\n",
			bound_ev);
		return TEST_RESULT_FAIL;
	}

	/* use a shared interrupt source for this test-case */
	private_interrupt = 0;

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)sdei_event, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x0x%llx\n",
			    (unsigned long long)target_mpid);
			goto err0;
		}
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
		    PSCI_STATE_OFF)
			continue;
		cpu_count--;
	}

	assert(cpu_count == 0);

	if (sdei_event() != TEST_RESULT_SUCCESS)
		goto err0;

	cpu_count--;
	assert(cpu_count == 0);

	sdei_interrupt_release(bound_ev, &intr_ctx);
	enable_irq();

	return TEST_RESULT_SUCCESS;

err0:
	sdei_private_reset();
	sdei_shared_reset();
	sdei_interrupt_release(bound_ev, &intr_ctx);
	enable_irq();
	return TEST_RESULT_FAIL;
}

/* Handle an SDEI event on all cores in parallel. */
test_result_t test_sdei_event_parallel(void)
{
	struct sdei_intr_ctx intr_ctx;
	u_register_t lead_mpid, target_mpid;
	int cpu_node;
	long long ret;

	lead_mpid = read_mpidr_el1() & MPID_MASK;
	participating_cpu_count = tftf_get_total_cpus_count();
	init_spinlock(&cpu_count_lock);
	cpu_count = 0;

	ret = sdei_version();
	if (ret != MAKE_SDEI_VERSION(1, 0, 0)) {
		tftf_testcase_printf("Unexpected SDEI version: 0x%llx\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	disable_irq();
	bound_ev = sdei_interrupt_bind(IRQ_PCPU_HP_TIMER, &intr_ctx);
	if (bound_ev < 0) {
		tftf_testcase_printf("SDEI interrupt bind failed: %x\n",
			bound_ev);
		return TEST_RESULT_FAIL;
	}

	/* use a private interrupt source for this test-case */
	private_interrupt = 1;

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)sdei_event, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x0x%llx\n",
			    (unsigned long long)target_mpid);
			goto err0;
		}
	}

	if (sdei_event() != TEST_RESULT_SUCCESS)
		goto err0;

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
			PSCI_STATE_OFF)
			continue;
		cpu_count--;
	}

	cpu_count--;
	assert(cpu_count == 0);

	sdei_interrupt_release(bound_ev, &intr_ctx);
	enable_irq();

	return TEST_RESULT_SUCCESS;
err0:
	sdei_private_reset();
	sdei_shared_reset();
	sdei_interrupt_release(bound_ev, &intr_ctx);
	enable_irq();
	return TEST_RESULT_FAIL;
}

static test_result_t sdei_event_signal_self(void)
{
	long long ret;

	ret = sdei_event_register(0, sdei_entrypoint_resume, EV_COOKIE,
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

	ret = sdei_event_signal(read_mpidr_el1());
	if (ret < 0) {
		tftf_testcase_printf("SDEI event signal failed: 0x%llx\n", ret);
		goto err2;
	}

	sdei_handler_done();

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

/* Each core signals itself using SDEI event signalling. */
test_result_t test_sdei_event_signal_serial(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node;
	long long ret;

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	ret = sdei_version();
	if (ret != MAKE_SDEI_VERSION(1, 0, 0)) {
		tftf_testcase_printf("Unexpected SDEI version: 0x%llx\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	disable_irq();
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)sdei_event_signal_self, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x0x%llx\n",
			    (unsigned long long)target_mpid);
			ret = -1;
			goto err0;
		}
		while (tftf_psci_affinity_info(target_mpid, MPIDR_AFFLVL0) !=
			PSCI_STATE_OFF)
			continue;
	}

	if (sdei_event_signal_self() != TEST_RESULT_SUCCESS) {
		ret = -1;
		goto err0;
	}

err0:
	enable_irq();
	if (ret < 0)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
}

static event_t cpu_ready[PLATFORM_CORE_COUNT];

static test_result_t sdei_wait_for_event_signal(void)
{
	int core_pos;
	long long ret;

	ret = sdei_event_register(0, sdei_entrypoint_resume, EV_COOKIE,
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

	core_pos = platform_get_core_pos(read_mpidr_el1());
	tftf_send_event(&cpu_ready[core_pos]);

	sdei_handler_done();

	sdei_pe_mask();
err1:
	sdei_event_disable(0);
err0:
	sdei_event_unregister(0);

	if (ret < 0)
		return TEST_RESULT_FAIL;

	return TEST_RESULT_SUCCESS;
}

/*
 * The primary core will signal all other cores
 * use SDEI event signalling.
 */
test_result_t test_sdei_event_signal_all(void)
{
	u_register_t lead_mpid, target_mpid;
	int cpu_node, core_pos;
	int i;
	long long ret;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++)
		tftf_init_event(&cpu_ready[i]);

	lead_mpid = read_mpidr_el1() & MPID_MASK;

	ret = sdei_version();
	if (ret != MAKE_SDEI_VERSION(1, 0, 0)) {
		tftf_testcase_printf("Unexpected SDEI version: 0x%llx\n", ret);
		return TEST_RESULT_SKIPPED;
	}

	disable_irq();
	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		ret = tftf_cpu_on(target_mpid,
		    (uintptr_t)sdei_wait_for_event_signal, 0);
		if (ret != PSCI_E_SUCCESS) {
			ERROR("CPU ON failed for 0x0x%llx\n",
			    (unsigned long long)target_mpid);
			ret = -1;
			goto err0;
		}
		core_pos = platform_get_core_pos(target_mpid);
		tftf_wait_for_event(&cpu_ready[core_pos]);
	}

	for_each_cpu(cpu_node) {
		target_mpid = tftf_get_mpidr_from_node(cpu_node) & MPID_MASK;
		if (lead_mpid == target_mpid)
			continue;
		ret = sdei_event_signal(target_mpid);
		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: 0x%llx\n",
				ret);
			ret = -1;
			goto err0;
		}
	}

err0:
	enable_irq();
	if (ret < 0)
		return TEST_RESULT_FAIL;
	return TEST_RESULT_SUCCESS;
}
