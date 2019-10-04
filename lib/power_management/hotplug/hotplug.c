/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <cdefs.h>		/* For __dead2 */
#include <debug.h>
#include <drivers/arm/arm_gic.h>
#include <drivers/console.h>
#include <irq.h>
#include <pauth.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <psci.h>
#include <sgi.h>
#include <spinlock.h>
#include <stdint.h>
#include <tftf.h>

/*
 * Affinity info map of CPUs as seen by TFTF
 *   - Set cpus_status_map[i].state to TFTF_AFFINITY_STATE_ON to mark CPU i
 *     as ON.
 *   - Set cpus_status_map[i].state to TFTF_AFFINITY_STATE_ON_PENDING to mark
 *     CPU i as ON_PENDING.
 *   - Set cpus_status_map[i].state to TFTF_AFFINITY_STATE_OFF to mark CPU i
 *     as OFF.
 */
static tftf_cpu_state_t cpus_status_map[PLATFORM_CORE_COUNT];
static int cpus_status_init_done;

/*
 * Reference count keeping track of the number of CPUs participating in
 * a test.
 */
static volatile unsigned int ref_cnt;

/* Lock to prevent concurrent accesses to the reference count */
static spinlock_t ref_cnt_lock;

/* Per-cpu test entrypoint */
volatile test_function_t test_entrypoint[PLATFORM_CORE_COUNT];

u_register_t tftf_primary_core = INVALID_MPID;

unsigned int tftf_inc_ref_cnt(void)
{
	unsigned int cnt;

	spin_lock(&ref_cnt_lock);
	assert(ref_cnt < PLATFORM_CORE_COUNT);
	cnt = ++ref_cnt;
	spin_unlock(&ref_cnt_lock);

	VERBOSE("Entering the test (%u CPUs in the test now)\n", cnt);

	return cnt;
}

unsigned int tftf_dec_ref_cnt(void)
{
	unsigned int cnt;

	spin_lock(&ref_cnt_lock);
	assert(ref_cnt != 0);
	cnt = --ref_cnt;
	spin_unlock(&ref_cnt_lock);

	VERBOSE("Exiting the test  (%u CPUs in the test now)\n", cnt);

	return cnt;
}

unsigned int tftf_get_ref_cnt(void)
{
	return ref_cnt;
}

void tftf_init_cpus_status_map(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);

	/* Check only primary does the initialisation */
	assert((mpid & MPID_MASK) == tftf_primary_core);

	/* Check init is done only once */
	assert(!cpus_status_init_done);

	cpus_status_init_done = 1;

	/*
	 * cpus_status_map already initialised to zero as part of BSS init,
	 * just set the primary to ON state
	 */
	cpus_status_map[core_pos].state = TFTF_AFFINITY_STATE_ON;
}

void tftf_set_cpu_online(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);

	/*
	 * Wait here till the `tftf_try_cpu_on` has had a chance to update the
	 * the cpu state.
	 */
	while (cpus_status_map[core_pos].state == TFTF_AFFINITY_STATE_OFF)
		;

	spin_lock(&cpus_status_map[core_pos].lock);
	assert(cpus_status_map[core_pos].state == TFTF_AFFINITY_STATE_ON_PENDING);
	cpus_status_map[core_pos].state = TFTF_AFFINITY_STATE_ON;
	spin_unlock(&cpus_status_map[core_pos].lock);
}

void tftf_set_cpu_offline(void)
{
	unsigned int mpid = read_mpidr_el1();
	unsigned int core_pos = platform_get_core_pos(mpid);

	spin_lock(&cpus_status_map[core_pos].lock);

	assert(tftf_is_cpu_online(mpid));
	cpus_status_map[core_pos].state = TFTF_AFFINITY_STATE_OFF;
	spin_unlock(&cpus_status_map[core_pos].lock);
}

unsigned int tftf_is_cpu_online(unsigned int mpid)
{
	unsigned int core_pos = platform_get_core_pos(mpid);
	return cpus_status_map[core_pos].state == TFTF_AFFINITY_STATE_ON;
}

unsigned int tftf_is_core_pos_online(unsigned int core_pos)
{
	return cpus_status_map[core_pos].state == TFTF_AFFINITY_STATE_ON;
}

int32_t tftf_cpu_on(u_register_t target_cpu,
		    uintptr_t entrypoint,
		    u_register_t context_id)
{
	int32_t ret;
	tftf_affinity_info_t cpu_state;
	unsigned int core_pos = platform_get_core_pos(target_cpu);

	spin_lock(&cpus_status_map[core_pos].lock);
	cpu_state = cpus_status_map[core_pos].state;

	if (cpu_state == TFTF_AFFINITY_STATE_ON) {
		spin_unlock(&cpus_status_map[core_pos].lock);
		return PSCI_E_ALREADY_ON;
	}

	if (cpu_state == TFTF_AFFINITY_STATE_ON_PENDING) {
		spin_unlock(&cpus_status_map[core_pos].lock);
		return PSCI_E_SUCCESS;
	}

	assert(cpu_state == TFTF_AFFINITY_STATE_OFF);

	do {
		ret = tftf_psci_cpu_on(target_cpu,
			       (uintptr_t) tftf_hotplug_entry,
			       context_id);

		/* Check if multiple CPU_ON calls are done for same CPU */
		assert(ret != PSCI_E_ON_PENDING);
	} while (ret == PSCI_E_ALREADY_ON);

	if (ret == PSCI_E_SUCCESS) {
		/*
		 * Populate the test entry point for this core.
		 * This is the address where the core will jump to once the framework
		 * has finished initialising it.
		 */
		test_entrypoint[core_pos] = (test_function_t) entrypoint;

		cpus_status_map[core_pos].state = TFTF_AFFINITY_STATE_ON_PENDING;
		spin_unlock(&cpus_status_map[core_pos].lock);
	} else {
		spin_unlock(&cpus_status_map[core_pos].lock);
		ERROR("Failed to boot CPU 0x%llx (%d)\n",
				(unsigned long long)target_cpu, ret);
	}

	return ret;
}

int32_t tftf_try_cpu_on(u_register_t target_cpu,
			uintptr_t entrypoint,
			u_register_t context_id)
{
	int32_t ret;
	unsigned int core_pos = platform_get_core_pos(target_cpu);

	ret = tftf_psci_cpu_on(target_cpu,
		       (uintptr_t) tftf_hotplug_entry,
		       context_id);

	if (ret == PSCI_E_SUCCESS) {
		spin_lock(&cpus_status_map[core_pos].lock);
		assert(cpus_status_map[core_pos].state ==
						TFTF_AFFINITY_STATE_OFF);
		cpus_status_map[core_pos].state =
				TFTF_AFFINITY_STATE_ON_PENDING;

		spin_unlock(&cpus_status_map[core_pos].lock);

		/*
		 * Populate the test entry point for this core.
		 * This is the address where the core will jump to once the
		 * framework has finished initialising it.
		 */
		test_entrypoint[core_pos] = (test_function_t) entrypoint;
	}

	return ret;
}

/*
 * Prepare the core to power off. Any driver which needs to perform specific
 * tasks before powering off a CPU, e.g. migrating interrupts to another
 * core, can implement a function and call it from here.
 */
static void tftf_prepare_cpu_off(void)
{
	/*
	 * Do the bare minimal to turn off this CPU i.e. turn off interrupts
	 * and disable the GIC CPU interface
	 */
	disable_irq();
	arm_gic_disable_interrupts_local();
}

/*
 * Revert the changes made during tftf_prepare_cpu_off()
 */
static void tftf_revert_cpu_off(void)
{
	arm_gic_enable_interrupts_local();
	enable_irq();
}

int32_t tftf_cpu_off(void)
{
	int32_t ret;

	tftf_prepare_cpu_off();
	tftf_set_cpu_offline();

	INFO("Powering off\n");

	/* Flush console before the last CPU is powered off. */
	if (tftf_get_ref_cnt() == 0)
		console_flush();

	/* Power off the CPU */
	ret = tftf_psci_cpu_off();

	ERROR("Failed to power off (%d)\n", ret);

	/*
	 * PSCI CPU_OFF call does not return when successful.
	 * Otherwise, it should return the PSCI error code 'DENIED'.
	 */
	assert(ret == PSCI_E_DENIED);

	/*
	 * The CPU failed to power down since we returned from
	 * tftf_psci_cpu_off(). So we need to adjust the framework's view of
	 * the core by marking it back online.
	 */
	tftf_set_cpu_online();
	tftf_revert_cpu_off();

	return ret;
}

/*
 * C entry point for a CPU that has just been powered up.
 */
void __dead2 tftf_warm_boot_main(void)
{
	/* Initialise the CPU */
	tftf_arch_setup();

#if ENABLE_PAUTH
	/*
	 * Program APIAKey_EL1 key and enable ARMv8.3-PAuth here as this
	 * function doesn't return, and RETAA instuction won't be executed,
	 * what would cause translation fault otherwise.
	 */
	pauth_init_enable();
#endif /* ENABLE_PAUTH */

	arm_gic_setup_local();

	/* Enable the SGI used by the timer management framework */
	tftf_irq_enable(IRQ_WAKE_SGI, GIC_HIGHEST_NS_PRIORITY);

	enable_irq();

	INFO("Booting\n");

	tftf_set_cpu_online();

	/* Enter the test session */
	run_tests();

	/* Should never reach this point */
	bug_unreachable();
}
