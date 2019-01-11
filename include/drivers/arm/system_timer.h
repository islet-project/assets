/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SYSTEM_TIMER_H__
#define __SYSTEM_TIMER_H__

#include <stdint.h>

/*
 * Program systimer to fire an interrupt after time_out_ms
 *
 * Always return 0
 */
int program_systimer(unsigned long time_out_ms);
/*
 * Cancel the currently programmed systimer interrupt
 *
 * Always return 0
 */
int cancel_systimer(void);
/*
 * Initialises the systimer so that it can be used for programming timer
 * interrupt.
 * Must be called by the primary CPU only.
 *
 * Always return 0
 */
int init_systimer(uintptr_t systimer_base);
/*
 * Handler to acknowledge and de-activate the systimer interrupt
 *
 * Always return 0
 */
int handler_systimer(void);

#endif /* __SYSTEM_TIMER_H__ */
