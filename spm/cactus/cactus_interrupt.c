/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
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

extern ffa_id_t g_ffa_id;

void cactus_interrupt_handler(void)
{
	uint32_t intid = spm_interrupt_get();

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
		NOTICE("Trusted WatchDog timer stopped\n");
		sp805_twdog_stop();

		/* Perform secure interrupt de-activation. */
		spm_interrupt_deactivate(intid);

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
}
