/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SP804_H__
#define __SP804_H__

#include <stdint.h>

#define SP804_LOAD_OFFSET		0x0
#define SP804_CURRENT_VALUE_OFFSET	0x4
#define SP804_CTRL_OFFSET		0x8
#define SP804_INT_CLR_OFFSET		0xC
#define SP804_INT_STATUS_OFFSET		0x10
#define SP804_MASKED_INT_STATUS_OFFSET	0x14
#define SP804_BG_LOAD_OFFSET		0x18

/* SP804 Timer control register bit-fields */
#define ONESHOT_MODE	(0x1 << 0)	/* Bit [0] */
#define TIMER_SIZE	(0x1 << 1)	/* Bit [1] */
#define TIMER_PRE_DIV1	(0x00 << 2)	/* Bits [2:3] */
#define INT_ENABLE	(0x01 << 5)	/* Bit [5] */
#define TIMER_MODE_FREE_RUN	(0x0 << 6)	/* Bit [6] */
#define TIMER_EN	(0x01 << 7)	/* Bit [7] */

/*
 * Program sp804 timer to fire an interrupt after `time_out_ms` milliseconds.
 *
 * Always return 0
 */
int sp804_timer_program(unsigned long time_out_ms);

/*
 * Cancel the currently programmed sp804 timer interrupt
 *
 * Always return 0
 */
int sp804_timer_cancel(void);

/*
 * Initializes the sp804 timer so that it can be used for programming
 * timer interrupt.
 * Must be called by the primary CPU only.
 *
 * Always return 0
 */
int sp804_timer_init(uintptr_t base_addr, unsigned int timer_freq);

/*
 * Handler to acknowledge and de-activate the sp804 timer interrupt
 *
 * Always return 0
 */
int sp804_timer_handler(void);

#endif /* __SP804_H__ */
