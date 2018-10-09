/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TIMER_H__
#define __TIMER_H__

#include <irq.h>

typedef struct plat_timer {
	int (*program)(unsigned long time_out_ms);
	int (*cancel)(void);
	int (*handler)(void);

	/*
	 * Duration of the atomic time slice in milliseconds. All timer
	 * requests within the same time slice are merged into one. This value
	 * should be chosen such that it is greater than the time required to
	 * program the timer.
	 */
	unsigned int timer_step_value;
	unsigned int timer_irq;
} plat_timer_t;

/*
 * Gets the platform specific timer implementation information and initialises
 * the timer framework and peripheral.
 * Returns 0 on success or return value of timer peripheral intialisation
 * function.
 */
int tftf_initialise_timer(void);

/*
 * Requests the timer framework to send an interrupt after milli_secs.
 * The interrupt is sent to the calling core of this api. The actual
 * time the interrupt is received by the core can be greater than
 * the requested time.
 * Returns 0 on success and -1 on failure.
 */
int tftf_program_timer(unsigned long milli_secs);

/*
 * Requests the timer framework to send an interrupt after milli_secs and to
 * suspend the CPU to the desired power state. The interrupt is sent to the
 * calling core of this api. The actual time the interrupt is received by the
 * core can be greater than the requested time.
 *
 * Return codes from tftf_program_timer calls and tftf_cpu_suspend are stored
 * respectively in timer_rc and suspend_rc output parameters.
 * If a function is not executed, the return value stored in the output
 * parameters will be as if the correponding call succeeded. NULL pointers are
 * accepted to discard the return codes.
 * Returns 0 on success and -1 on failure.
 */

int tftf_program_timer_and_suspend(unsigned long milli_secs,
				   unsigned int pwr_state,
				   int *timer_rc, int *suspend_rc);

/*
 * Requests the timer framework to send an interrupt after milli_secs and to
 * suspend the system. The interrupt is sent to the calling core of this api.
 * The actual time the interrupt is received by the core can be greater than
 * the requested time. For the system suspend to succeed, all cores other than
 * the calling core should be in the OFF state.
 *
 * Return codes from tftf_program_timer calls and tftf_cpu_system suspend
 * are stored respectively in timer_rc and suspend_rc output parameters.
 * If a function is not executed, the return value stored in the output
 * parameters will be as if the correponding call succeeded. NULL pointers are
 * accepted to discard the return codes.
 * Returns 0 on success and -1 on failure.
 */
int tftf_program_timer_and_sys_suspend(unsigned long milli_secs,
					   int *timer_rc, int *suspend_rc);

/*
 * Suspends the calling CPU for specified milliseconds.
 *
 * Returns 0 on success, and -1 otherwise.
 */
int tftf_timer_sleep(unsigned long milli_secs);

/*
 * Common handler for servicing all the timer interrupts. It in turn calls the
 * peripheral specific handler. It also sends WAKE_SGI to all the cores which
 * requested an interrupt within a time frame of timer_step_value.
 * Also, if there are pending interrupt requests, reprograms the timer
 * accordingly to fire an interrupt at the right time.
 *
 * Returns 0 on success.
 */
int tftf_timer_framework_handler(void *data);

/*
 * Cancels the previously programmed value by the called core.
 * This api should be used only for cancelling the self interrupt request
 * by a core.
 * Returns 0 on success, negative value otherwise.
 */
int tftf_cancel_timer(void);

/*
 * It is used to register a handler which needs to be called when a timer
 * interrupt is fired.
 * Returns 0 on success, negative value otherwise.
 */
int tftf_timer_register_handler(irq_handler_t irq_handler);

/*
 * It is used to unregister a previously registered handler.
 * Returns 0 on success, negative value otherwise.
 */
int tftf_timer_unregister_handler(void);

/*
 * Return the IRQ Number of the registered timer interrupt
 */
unsigned int tftf_get_timer_irq(void);

/*
 * Returns the timer step value in a platform and is used by test cases.
 */
unsigned int tftf_get_timer_step_value(void);

/*
 * Restore the GIC state after wake-up from system suspend
 */
void tftf_timer_gic_state_restore(void);

#endif /* __TIMER_H__ */
