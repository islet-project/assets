/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>

#include <ffa_helpers.h>
#include <sp_helpers.h>

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

	ERROR("%s: Interrupt ID %u not handled!\n", __func__, fiq_num);

	return 0;
}
