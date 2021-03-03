/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>

#include <ffa_helpers.h>
#include <sp_helpers.h>
#include <spm_helpers.h>

#include "cactus_test_cmds.h"
#include "spm_common.h"

extern ffa_vm_id_t g_ffa_id;

static void managed_exit_handler(void)
{
	/*
	 * Real SP will save its context here.
	 * Send interrupt ID for acknowledgement
	 */
	cactus_response(g_ffa_id, HYP_ID, MANAGED_EXIT_INTERRUPT_ID);
}

int cactus_irq_handler(void)
{
	uint32_t irq_num;

	irq_num = spm_interrupt_get();

	ERROR("%s: Interrupt ID %u not handled!\n", __func__, irq_num);

	return 0;
}

int cactus_fiq_handler(void)
{
	uint32_t fiq_num;

	fiq_num = spm_interrupt_get();

	if (fiq_num == MANAGED_EXIT_INTERRUPT_ID) {
		managed_exit_handler();
	} else {
		ERROR("%s: Interrupt ID %u not handled!\n", __func__, fiq_num);
	}

	return 0;
}
