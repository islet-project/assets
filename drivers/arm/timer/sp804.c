/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <drivers/arm/arm_gic.h>
#include <drivers/arm/gic_v2.h>
#include <drivers/arm/sp804.h>
#include <mmio.h>

static unsigned int sp804_freq;
static uintptr_t sp804_base;

int sp804_timer_program(unsigned long time_out_ms)
{
	unsigned int load_val;
	unsigned char ctrl_reg;

	assert(sp804_base);
	assert(time_out_ms);

	/* Disable the timer */
	ctrl_reg = mmio_read_8(sp804_base + SP804_CTRL_OFFSET);
	ctrl_reg &= ~(TIMER_EN | INT_ENABLE);
	mmio_write_8(sp804_base + SP804_CTRL_OFFSET, ctrl_reg);

	/* Calculate the load value */
	load_val = (sp804_freq * time_out_ms) / 1000;

	/* Write the load value to sp804 timer */
	mmio_write_32(sp804_base + SP804_LOAD_OFFSET, load_val);

	/* Enable the timer */
	ctrl_reg |= (TIMER_EN | INT_ENABLE);
	mmio_write_8(sp804_base + SP804_CTRL_OFFSET, ctrl_reg);

	return 0;
}

static void sp804_timer_disable(void)
{
	unsigned char ctrl_reg;

	/*
	 * The interrupt line should be cleared prior to timer disable.
	 * Otherwise the interrupt line level decay from high to quiescent
	 * level is not quick enough which may trigger spurious interrupt.
	 * Write a dummy load value to sp804 timer to clear the interrupt.
	 */
	mmio_write_32(sp804_base + SP804_LOAD_OFFSET, 0xffff);

	/* De-assert the timer interrupt */
	mmio_write_8(sp804_base + SP804_INT_CLR_OFFSET, 0x0);

	/* Disable the timer */
	ctrl_reg = mmio_read_8(sp804_base + SP804_CTRL_OFFSET);
	ctrl_reg &= ~(TIMER_EN | INT_ENABLE);
	mmio_write_8(sp804_base + SP804_CTRL_OFFSET, ctrl_reg);
}

int sp804_timer_cancel(void)
{
	assert(sp804_base);
	sp804_timer_disable();
	return 0;
}

int sp804_timer_handler(void)
{
	assert(sp804_base);
	sp804_timer_disable();
	return 0;
}

int sp804_timer_init(uintptr_t base_addr, unsigned int timer_freq)
{
	unsigned char ctrl_reg;

	/* Check input parameters */
	assert(base_addr && timer_freq);

	/* Check for duplicate initialization */
	assert(sp804_base == 0);

	sp804_base = base_addr;
	sp804_freq = timer_freq;

	/*
	 * Configure the timer in one shot mode, pre-scalar divider to 1,
	 * timer counter width to 32 bits and un-mask the interrupt.
	 */
	ctrl_reg = ONESHOT_MODE | TIMER_PRE_DIV1 | TIMER_SIZE;
	mmio_write_8(sp804_base + SP804_CTRL_OFFSET, ctrl_reg);

	return 0;
}
