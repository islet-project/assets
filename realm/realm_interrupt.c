/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>

/* dummy interrupt handler as for now*/
void realm_interrupt_handler(void)
{
	INFO("%s\n", __func__);
}
