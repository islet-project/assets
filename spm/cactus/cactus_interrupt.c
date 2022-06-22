/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>

#include "cactus_message_loop.h"
#include "cactus_test_cmds.h"
#include <drivers/arm/sp805.h>
#include <ffa_helpers.h>
#include <sp_helpers.h>
#include "spm_common.h"
#include <spm_helpers.h>

#include <platform_def.h>

#define NOTIFICATION_PENDING_INTERRUPT_INTID 5

extern void notification_pending_interrupt_handler(void);

extern ffa_id_t g_ffa_id;
static uint32_t managed_exit_interrupt_id;

/* Secure virtual interrupt that was last handled by Cactus SP. */
uint32_t last_serviced_interrupt[PLATFORM_CORE_COUNT];

extern spinlock_t sp_handler_lock[NUM_VINT_ID];

/*
 * Managed exit ID discoverable by querying the SPMC through
 * FFA_FEATURES API.
 */
void discover_managed_exit_interrupt_id(void)
{
	struct ffa_value ffa_ret;

	/* Interrupt ID value is returned through register W2. */
	ffa_ret = ffa_features(FFA_FEATURE_MEI);
	managed_exit_interrupt_id = ffa_feature_intid(ffa_ret);

	VERBOSE("Discovered managed exit interrupt ID: %d\n",
	     managed_exit_interrupt_id);
}

void cactus_interrupt_handler(void)
{
	uint32_t intid = spm_interrupt_get();
	unsigned int core_pos = get_current_core_id();

	switch (intid) {
	case MANAGED_EXIT_INTERRUPT_ID:
		/*
		 * A secure partition performs its housekeeping and sends a
		 * direct response to signal interrupt completion.
		 * This is a pure virtual interrupt, no need for deactivation.
		 */
		cactus_response(g_ffa_id, HYP_ID, MANAGED_EXIT_INTERRUPT_ID);
		break;
	case IRQ_TWDOG_INTID:
		/*
		 * Interrupt triggered due to Trusted watchdog timer expiry.
		 * Clear the interrupt and stop the timer.
		 */
		VERBOSE("Trusted WatchDog timer stopped\n");
		sp805_twdog_stop();

		/* Perform secure interrupt de-activation. */
		spm_interrupt_deactivate(intid);

		break;
	case NOTIFICATION_PENDING_INTERRUPT_INTID:
		notification_pending_interrupt_handler();
		break;
	default:
		/*
		 * Currently the only source of secure interrupt is Trusted
		 * Watchdog timer.
		 */
		ERROR("%s: Interrupt ID %x not handled!\n", __func__,
			 intid);
		panic();
	}

	last_serviced_interrupt[core_pos] = intid;

	/* Invoke the tail end handler registered by the SP. */
	spin_lock(&sp_handler_lock[intid]);
	if (sp_interrupt_tail_end_handler[intid]) {
		sp_interrupt_tail_end_handler[intid]();
	}
	spin_unlock(&sp_handler_lock[intid]);
}
