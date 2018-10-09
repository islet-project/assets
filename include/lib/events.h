/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <spinlock.h>

typedef struct {
	/*
	 * Counter that keeps track of the minimum number of recipients of the
	 * event. When the event is sent, this counter is incremented. When it
	 * is received, it is decremented. Therefore, a zero value means that
	 * the event hasn't been sent yet, or that all recipients have already
	 * received it.
	 *
	 * Volatile is needed as it will enforce ordering relatively to
	 * accesses to the lock.
	 */
	volatile unsigned int cnt;

	/* Lock used to avoid concurrent accesses to the counter */
	spinlock_t lock;
} event_t;

/*
 * Initialise an event.
 *   event: Address of the event to initialise
 *
 * This function can be used either to initialise a newly created event
 * structure or to recycle one.
 *
 * Note: This function is not MP-safe. It can't use the event lock as it is
 * responsible for initialising it. Care must be taken to ensure this function
 * is called in the right circumstances.
 */
void tftf_init_event(event_t *event);

/*
 * Send an event to a CPU.
 *   event: Address of the variable that acts as a synchronisation object.
 *
 * Which CPU receives the event is determined on a first-come, first-served
 * basis. If several CPUs are waiting for the same event then the first CPU
 * which takes the event will reflect that in the event structure.
 *
 * Note: This is equivalent to calling:
 *   tftf_send_event_to(event, 1);
 */
void tftf_send_event(event_t *event);

/*
 * Send an event to all CPUs.
 *   event: Address of the variable that acts as a synchronisation object.
 *
 * Note: This is equivalent to calling:
 *   tftf_send_event_to(event, PLATFORM_CORE_COUNT);
 */
void tftf_send_event_to_all(event_t *event);

/*
 * Send an event to a given number of CPUs.
 *   event: Address of the variable that acts as a synchronisation object.
 *   cpus_count: Number of CPUs to send the event to.
 *
 * Which CPUs receive the event is determined on a first-come, first-served
 * basis. If more than 'cpus_count' CPUs are waiting for the same event then the
 * first 'cpus_count' CPUs which take the event will reflect that in the event
 * structure.
 */
void tftf_send_event_to(event_t *event, unsigned int cpus_count);

/*
 * Wait for an event.
 *   event: Address of the variable that acts as a synchronisation object.
 */
void tftf_wait_for_event(event_t *event);

#endif /* __EVENTS_H__ */
