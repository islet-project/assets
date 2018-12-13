/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <arm_gic.h>
#include <debug.h>
#include <errno.h>
#include <irq.h>
#include <mmio.h>
#include <platform.h>
#include <platform_def.h>
#include <power_management.h>
#include <sgi.h>
#include <spinlock.h>
#include <stddef.h>
#include <sys/types.h>
#include <tftf.h>
#include <timer.h>


/* Helper macros */
#define TIMER_STEP_VALUE (plat_timer_info->timer_step_value)
#define TIMER_IRQ (plat_timer_info->timer_irq)
#define PROGRAM_TIMER(a) plat_timer_info->program(a)
#define INVALID_CORE	UINT32_MAX
#define INVALID_TIME	UINT64_MAX
#define MAX_TIME_OUT_MS	10000

/*
 * Pointer containing available timer information for the platform.
 */
static const plat_timer_t *plat_timer_info;
/*
 * Interrupt requested time by cores in terms of absolute time.
 */
static volatile unsigned long long interrupt_req_time[PLATFORM_CORE_COUNT];
/*
 * Contains the target core number of the timer interrupt.
 */
static unsigned int current_prog_core = INVALID_CORE;
/*
 * Lock to get a consistent view for programming the timer
 */
static spinlock_t timer_lock;
/*
 * Number of system ticks per millisec
 */
static unsigned int systicks_per_ms;

/*
 * Stores per CPU timer handler invoked on expiration of the requested timeout.
 */
static irq_handler_t timer_handler[PLATFORM_CORE_COUNT];

/* Helper function */
static inline unsigned long long get_current_time_ms(void)
{
	assert(systicks_per_ms);
	return mmio_read_64(SYS_CNT_BASE1 + CNTPCT_LO) / systicks_per_ms;
}

static inline unsigned long long get_current_prog_time(void)
{
	return current_prog_core == INVALID_CORE ?
		0 : interrupt_req_time[current_prog_core];
}

int tftf_initialise_timer(void)
{
	/*
	 * Get platform specific timer information
	 */
	int rc = plat_initialise_timer_ops(&plat_timer_info);
	if (rc != 0) {
		return rc;
	}

	/* Systems can't support single tick as a step value */
	assert(TIMER_STEP_VALUE);

	/* Initialise the array to max possible time */
	for (unsigned int i = 0; i < PLATFORM_CORE_COUNT; i++)
		interrupt_req_time[i] = INVALID_TIME;

	tftf_irq_register_handler(TIMER_IRQ, tftf_timer_framework_handler);
	arm_gic_set_intr_priority(TIMER_IRQ, GIC_HIGHEST_NS_PRIORITY);
	arm_gic_intr_enable(TIMER_IRQ);

	/* Save the systicks per millisecond */
	systicks_per_ms = read_cntfrq_el0() / 1000;

	return 0;
}

/*
 * It returns the core number of next timer request to be serviced or
 * -1 if there is no request from any core. The next service request
 * is the core whose interrupt needs to be fired first.
 */
static inline unsigned int get_lowest_req_core(void)
{
	unsigned long long lowest_timer = INVALID_TIME;
	unsigned int lowest_core_req = INVALID_CORE;
	unsigned int i;

	/*
	 * If 2 cores requested same value, give precedence
	 * to the core with lowest core number
	 */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (interrupt_req_time[i] < lowest_timer) {
			lowest_timer = interrupt_req_time[i];
			lowest_core_req =  i;
		}
	}

	return lowest_core_req;
}

int tftf_program_timer(unsigned long time_out_ms)
{
	unsigned int core_pos;
	unsigned long long current_time;
	u_register_t flags;
	int rc = 0;

	/*
	 * Some timer implementations have a very small max timeouts due to
	 * this if a request is asked for greater than the max time supported
	 * by them either it has to be broken down and remembered or use
	 * some other technique. Since that use case is not intended and
	 * and to make the timer framework simple, max timeout requests
	 * accepted by timer implementations can't be greater than
	 * 10 seconds. Hence, all timer peripherals used in timer framework
	 * has to support a timeout with interval of at least MAX_TIMEOUT.
	 */
	if ((time_out_ms > MAX_TIME_OUT_MS) || (time_out_ms == 0)) {
		ERROR("%s : Greater than max timeout request\n", __func__);
		return -1;
	} else if (time_out_ms < TIMER_STEP_VALUE) {
		time_out_ms = TIMER_STEP_VALUE;
	}

	core_pos = platform_get_core_pos(read_mpidr_el1());
	/* A timer interrupt request is already available for the core */
	assert(interrupt_req_time[core_pos] == INVALID_TIME);

	flags = read_daif();
	disable_irq();
	spin_lock(&timer_lock);

	assert((current_prog_core < PLATFORM_CORE_COUNT) ||
		(current_prog_core == INVALID_CORE));

	/*
	 * Read time after acquiring timer_lock to account for any time taken
	 * by lock contention.
	 */
	current_time = get_current_time_ms();

	/* Update the requested time */
	interrupt_req_time[core_pos] = current_time + time_out_ms;

	VERBOSE("Need timer interrupt at: %lld current_prog_time:%lld\n"
			" current time: %lld\n", interrupt_req_time[core_pos],
					get_current_prog_time(),
					get_current_time_ms());

	/*
	 * If the interrupt request time is less than the current programmed
	 * by timer_step_value or timer is not programmed. Program it with
	 * requested time and retarget the timer interrupt to the current
	 * core.
	 */
	if ((!get_current_prog_time()) || (interrupt_req_time[core_pos] <
				(get_current_prog_time() - TIMER_STEP_VALUE))) {

		arm_gic_set_intr_target(TIMER_IRQ, core_pos);

		rc = PROGRAM_TIMER(time_out_ms);
		/* We don't expect timer programming to fail */
		if (rc)
			ERROR("%s %d: rc = %d\n", __func__, __LINE__, rc);

		current_prog_core = core_pos;
	}

	spin_unlock(&timer_lock);
	/* Restore DAIF flags */
	write_daif(flags);
	isb();

	return rc;
}

int tftf_program_timer_and_suspend(unsigned long milli_secs,
				   unsigned int pwr_state,
				   int *timer_rc, int *suspend_rc)
{
	int rc = 0;
	u_register_t flags;

	/* Default to successful return codes */
	int timer_rc_val = 0;
	int suspend_rc_val = PSCI_E_SUCCESS;

	/* Preserve DAIF flags. IRQs need to be disabled for this to work. */
	flags = read_daif();
	disable_irq();

	/*
	 * Even with IRQs masked, the timer IRQ will wake the CPU up.
	 *
	 * If the timer IRQ happens before entering suspend mode (because the
	 * timer took too long to program, for example) the fact that the IRQ is
	 * pending will prevent the CPU from entering suspend mode and not being
	 * able to wake up.
	 */
	timer_rc_val = tftf_program_timer(milli_secs);
	if (timer_rc_val == 0) {
		suspend_rc_val = tftf_cpu_suspend(pwr_state);
		if (suspend_rc_val != PSCI_E_SUCCESS) {
			rc = -1;
			INFO("%s %d: suspend_rc = %d\n", __func__, __LINE__,
				suspend_rc_val);
		}
	} else {
		rc = -1;
		INFO("%s %d: timer_rc = %d\n", __func__, __LINE__, timer_rc_val);
	}

	/* Restore previous DAIF flags */
	write_daif(flags);
	isb();

	if (timer_rc)
		*timer_rc = timer_rc_val;
	if (suspend_rc)
		*suspend_rc = suspend_rc_val;
	/*
	 * If IRQs were disabled when calling this function, the timer IRQ
	 * handler won't be called and the timer interrupt will be pending, but
	 * that isn't necessarily a problem.
	 */

	return rc;
}

int tftf_program_timer_and_sys_suspend(unsigned long milli_secs,
				   int *timer_rc, int *suspend_rc)
{
	int rc = 0;
	u_register_t flags;

	/* Default to successful return codes */
	int timer_rc_val = 0;
	int suspend_rc_val = PSCI_E_SUCCESS;

	/* Preserve DAIF flags. IRQs need to be disabled for this to work. */
	flags = read_daif();
	disable_irq();

	/*
	 * Even with IRQs masked, the timer IRQ will wake the CPU up.
	 *
	 * If the timer IRQ happens before entering suspend mode (because the
	 * timer took too long to program, for example) the fact that the IRQ is
	 * pending will prevent the CPU from entering suspend mode and not being
	 * able to wake up.
	 */
	timer_rc_val = tftf_program_timer(milli_secs);
	if (timer_rc_val == 0) {
		suspend_rc_val = tftf_system_suspend();
		if (suspend_rc_val != PSCI_E_SUCCESS) {
			rc = -1;
			INFO("%s %d: suspend_rc = %d\n", __func__, __LINE__,
				suspend_rc_val);
		}
	} else {
		rc = -1;
		INFO("%s %d: timer_rc = %d\n", __func__, __LINE__, timer_rc_val);
	}

	/* Restore previous DAIF flags */
	write_daif(flags);
	isb();

	/*
	 * If IRQs were disabled when calling this function, the timer IRQ
	 * handler won't be called and the timer interrupt will be pending, but
	 * that isn't necessarily a problem.
	 */
	if (timer_rc)
		*timer_rc = timer_rc_val;
	if (suspend_rc)
		*suspend_rc = suspend_rc_val;

	return rc;
}

int tftf_timer_sleep(unsigned long milli_secs)
{
	int ret, power_state;
	uint32_t stateid;

	ret = tftf_psci_make_composite_state_id(MPIDR_AFFLVL0,
			PSTATE_TYPE_STANDBY, &stateid);
	if (ret != PSCI_E_SUCCESS)
		return -1;

	power_state = tftf_make_psci_pstate(MPIDR_AFFLVL0, PSTATE_TYPE_STANDBY,
			stateid);
	ret = tftf_program_timer_and_suspend(milli_secs, power_state,
								NULL, NULL);
	if (ret != 0)
		return -1;

	return 0;
}

int tftf_cancel_timer(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned int next_timer_req_core_pos;
	unsigned long long current_time;
	u_register_t flags;
	int rc = 0;

	/*
	 * IRQ is disabled so that if a timer is fired after taking a lock,
	 * it will remain pending and a core does not hit IRQ handler trying
	 * to acquire an already locked spin_lock causing dead lock.
	 */
	flags = read_daif();
	disable_irq();
	spin_lock(&timer_lock);

	interrupt_req_time[core_pos] = INVALID_TIME;

	if (core_pos == current_prog_core) {
		/*
		 * Cancel the programmed interrupt at the peripheral. If the
		 * timer interrupt is level triggered and fired this also
		 * deactivates the pending interrupt.
		 */
		rc = plat_timer_info->cancel();
		/* We don't expect cancel timer to fail */
		if (rc) {
			ERROR("%s %d: rc = %d\n", __func__, __LINE__, rc);
			goto exit;
		}

		/*
		 * For edge triggered interrupts, if an IRQ is fired before
		 * cancel timer is executed, the signal remains pending. So,
		 * clear the Timer IRQ if it is already pending.
		 */
		if (arm_gic_is_intr_pending(TIMER_IRQ))
			arm_gic_intr_clear(TIMER_IRQ);

		/* Get next timer consumer */
		next_timer_req_core_pos = get_lowest_req_core();
		if (next_timer_req_core_pos != INVALID_CORE) {

			/* Retarget to the next_timer_req_core_pos */
			arm_gic_set_intr_target(TIMER_IRQ, next_timer_req_core_pos);
			current_prog_core = next_timer_req_core_pos;

			current_time = get_current_time_ms();

			/*
			 * If the next timer request is lesser than or in a
			 * window of TIMER_STEP_VALUE from current time,
			 * program it to fire after TIMER_STEP_VALUE.
			 */
			if (interrupt_req_time[next_timer_req_core_pos] >
					 current_time + TIMER_STEP_VALUE)
				rc = PROGRAM_TIMER(interrupt_req_time[next_timer_req_core_pos] - current_time);
			else
				rc = PROGRAM_TIMER(TIMER_STEP_VALUE);
			VERBOSE("Cancel and program new timer for core_pos: "
						"%d %lld\n",
						next_timer_req_core_pos,
						get_current_prog_time());
			/* We don't expect timer programming to fail */
			if (rc)
				ERROR("%s %d: rc = %d\n", __func__, __LINE__, rc);
		} else {
			current_prog_core = INVALID_CORE;
			VERBOSE("Cancelling timer : %d\n", core_pos);
		}
	}
exit:
	spin_unlock(&timer_lock);

	/* Restore DAIF flags */
	write_daif(flags);
	isb();

	return rc;
}

int tftf_timer_framework_handler(void *data)
{
	unsigned int handler_core_pos = platform_get_core_pos(read_mpidr_el1());
	unsigned int next_timer_req_core_pos;
	unsigned long long current_time;
	int rc = 0;

	assert(interrupt_req_time[handler_core_pos] != INVALID_TIME);
	spin_lock(&timer_lock);

	current_time = get_current_time_ms();
	/* Check if we interrupt is targeted correctly */
	assert(handler_core_pos == current_prog_core);

	interrupt_req_time[handler_core_pos] = INVALID_TIME;

	/* Execute the driver handler */
	if (plat_timer_info->handler)
		plat_timer_info->handler();

	if (arm_gic_is_intr_pending(TIMER_IRQ)) {
		/*
		 * We might never manage to acquire the printf lock here
		 * (because we are in ISR context) but we're gonna panic right
		 * after anyway so it doesn't really matter.
		 */
		ERROR("Timer IRQ still pending. Fatal error.\n");
		panic();
	}

	/*
	 * Execute the handler requested by the core, the handlers for the
	 * other cores will be executed as part of handling IRQ_WAKE_SGI.
	 */
	if (timer_handler[handler_core_pos])
		timer_handler[handler_core_pos](data);

	/* Send interrupts to all the CPUS in the min time block */
	for (int i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if ((interrupt_req_time[i] <=
				(current_time + TIMER_STEP_VALUE))) {
			interrupt_req_time[i] = INVALID_TIME;
			tftf_send_sgi(IRQ_WAKE_SGI, i);
		}
	}

	/* Get the next lowest requested timer core and program it */
	next_timer_req_core_pos = get_lowest_req_core();
	if (next_timer_req_core_pos != INVALID_CORE) {
		/* Check we have not exceeded the time for next core */
		assert(interrupt_req_time[next_timer_req_core_pos] >
							current_time);
		arm_gic_set_intr_target(TIMER_IRQ, next_timer_req_core_pos);
		rc = PROGRAM_TIMER(interrupt_req_time[next_timer_req_core_pos]
				 - current_time);
	}
	/* Update current program core to the newer one */
	current_prog_core = next_timer_req_core_pos;

	spin_unlock(&timer_lock);

	return rc;
}

int tftf_timer_register_handler(irq_handler_t irq_handler)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	int ret;

	/* Validate no handler is registered */
	assert(!timer_handler[core_pos]);
	timer_handler[core_pos] = irq_handler;

	/*
	 * Also register same handler to IRQ_WAKE_SGI, as it can be waken
	 * by it.
	 */
	ret = tftf_irq_register_handler(IRQ_WAKE_SGI, irq_handler);
	assert(!ret);

	return ret;
}

int tftf_timer_unregister_handler(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	int ret;

	/*
	 * Unregister the handler for IRQ_WAKE_SGI also
	 */
	ret = tftf_irq_unregister_handler(IRQ_WAKE_SGI);
	assert(!ret);
	/* Validate a handler is registered */
	assert(timer_handler[core_pos]);
	timer_handler[core_pos] = 0;

	return ret;
}

unsigned int tftf_get_timer_irq(void)
{
	/*
	 * Check if the timer info is initialised
	 */
	assert(TIMER_IRQ);
	return TIMER_IRQ;
}

unsigned int tftf_get_timer_step_value(void)
{
	assert(TIMER_STEP_VALUE);

	return TIMER_STEP_VALUE;
}

/*
 * There are 4 cases that could happen when a system is resuming from system
 * suspend. The cases are:
 * 1. The resumed core is the last core to power down and the
 * timer interrupt was targeted to it.  In this case, target the
 * interrupt to our core and set the appropriate priority and enable it.
 *
 * 2. The resumed core was the last core to power down but the timer interrupt
 * is targeted to another core because of timer request grouping within
 * TIMER_STEP_VALUE. In this case, re-target the interrupt to our core
 * and set the appropriate priority and enable it
 *
 * 3. The system suspend request was down-graded by firmware and the timer
 * interrupt is targeted to another core which woke up first. In this case,
 * that core will wake us up and the interrupt_req_time[] corresponding to
 * our core will be cleared. In this case, no need to do anything as GIC
 * state is preserved.
 *
 * 4. The system suspend is woken up by another external interrupt other
 * than the timer framework interrupt. In this case, just enable the
 * timer interrupt and set the correct priority at GICD.
 */
void tftf_timer_gic_state_restore(void)
{
	unsigned int core_pos = platform_get_core_pos(read_mpidr_el1());
	spin_lock(&timer_lock);

	arm_gic_set_intr_priority(TIMER_IRQ, GIC_HIGHEST_NS_PRIORITY);
	arm_gic_intr_enable(TIMER_IRQ);

	/* Check if the programmed core is the woken up core */
	if (interrupt_req_time[core_pos] == INVALID_TIME) {
		INFO("The programmed core is not the one woken up\n");
	} else {
		current_prog_core = core_pos;
		arm_gic_set_intr_target(TIMER_IRQ, core_pos);
	}

	spin_unlock(&timer_lock);
}

