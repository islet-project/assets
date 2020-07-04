/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <assert.h>
#include <debug.h>
#include <events.h>
#include <platform_def.h>
#include <tftf.h>
#include <tftf_lib.h>

void tftf_init_event(event_t *event)
{
	assert(event != NULL);
	event->cnt = 0;
	event->lock.lock = 0;
}

static void send_event_common(event_t *event, unsigned int inc)
{
	spin_lock(&event->lock);
	event->cnt += inc;
	spin_unlock(&event->lock);

	/*
	 * Make sure the cnt increment is observable by all CPUs
	 * before the event is sent.
	 */
	dsbsy();
	sev();
}

void tftf_send_event(event_t *event)
{
	VERBOSE("Sending event %p\n", (void *) event);
	send_event_common(event, 1);
}

void tftf_send_event_to_all(event_t *event)
{
	VERBOSE("Sending event %p to all CPUs\n", (void *) event);
	send_event_common(event, PLATFORM_CORE_COUNT);
}

void tftf_send_event_to(event_t *event, unsigned int cpus_count)
{
	assert(cpus_count <= PLATFORM_CORE_COUNT);
	VERBOSE("Sending event %p to %u CPUs\n", (void *) event, cpus_count);
	send_event_common(event, cpus_count);
}

void tftf_wait_for_event(event_t *event)
{
	unsigned int event_received = 0;

	VERBOSE("Waiting for event %p\n", (void *) event);
	while (!event_received) {

		dsbsy();
		/* Wait for someone to send an event */
		if (!event->cnt) {
			wfe();
		} else {
			spin_lock(&event->lock);

			 /*
			  * Check that the event is still pending and that no
			  * one stole it from us while we were trying to
			  * acquire the lock.
			  */
			if (event->cnt != 0) {
				event_received = 1;
				--event->cnt;
			}
			/*
			 * No memory barrier is needed here because spin_unlock()
			 * issues a Store-Release instruction, which guarantees
			 * that loads and stores appearing in program order
			 * before the Store-Release are observed before the
			 * Store-Release itself.
			 */
			spin_unlock(&event->lock);
		}
	}

	VERBOSE("Received event %p\n", (void *) event);
}
