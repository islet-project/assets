/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <debug.h>
#include <drivers/arm/sp805.h>
#include <mmio.h>
#include <platform_def.h>
#include <stdint.h>

static inline uint32_t sp805_read_wdog_load(unsigned long base)
{
	assert(base);
	return mmio_read_32(base + SP805_WDOG_LOAD_OFF);
}

static inline void sp805_write_wdog_load(unsigned long base, uint32_t value)
{
	assert(base);
	mmio_write_32(base + SP805_WDOG_LOAD_OFF, value);
}

static inline uint32_t sp805_read_wdog_value(unsigned long base)
{
	assert(base);
	return mmio_read_32(base + SP805_WDOG_VALUE_0FF);
}

static inline uint32_t sp805_read_wdog_ctrl(unsigned long base)
{
	assert(base);
	return mmio_read_32(base + SP805_WDOG_CTRL_OFF) & SP805_WDOG_CTRL_MASK;
}

static inline void sp805_write_wdog_ctrl(unsigned long base, uint32_t value)
{
	assert(base);
	/* Not setting reserved bits */
	assert(!(value & ~SP805_WDOG_CTRL_MASK));
	mmio_write_32(base + SP805_WDOG_CTRL_OFF, value);
}

static inline void sp805_write_wdog_int_clr(unsigned long base, uint32_t value)
{
	assert(base);
	mmio_write_32(base + SP805_WDOG_INT_CLR_OFF, value);
}

static inline uint32_t sp805_read_wdog_ris(unsigned long base)
{
	assert(base);
	return mmio_read_32(base + SP805_WDOG_RIS_OFF) & SP805_WDOG_RIS_MASK;
}

static inline uint32_t sp805_read_wdog_mis(unsigned long base)
{
	assert(base);
	return mmio_read_32(base + SP805_WDOG_MIS_OFF) & SP805_WDOG_MIS_MASK;
}

static inline uint32_t sp805_read_wdog_lock(unsigned long base)
{
	assert(base);
	return mmio_read_32(base + SP805_WDOG_LOCK_OFF);
}

static inline void sp805_write_wdog_lock(unsigned long base, uint32_t value)
{
	assert(base);
	mmio_write_32(base + SP805_WDOG_LOCK_OFF, value);
}

static inline uint32_t sp805_read_wdog_itcr(unsigned long base)
{
	assert(base);
	return mmio_read_32(base + SP805_WDOG_ITCR_OFF) & SP805_WDOG_ITCR_MASK;
}

static inline void sp805_write_wdog_itcr(unsigned long base, uint32_t value)
{
	assert(base);
	/* Not setting reserved bits */
	assert(!(value & ~SP805_WDOG_ITCR_MASK));
	mmio_write_32(base + SP805_WDOG_ITCR_OFF, value);
}

static inline void sp805_write_wdog_itop(unsigned long base, uint32_t value)
{
	assert(base);
	/* Not setting reserved bits */
	assert(!(value & ~SP805_WDOG_ITOP_MASK));
	mmio_write_32(base + SP805_WDOG_ITOP_OFF, value);
}

static inline uint32_t sp805_read_wdog_periph_id(unsigned long base, unsigned int id)
{
	assert(base);
	assert(id < 4);
	return mmio_read_32(base + SP805_WDOG_PERIPH_ID_OFF + (id << 2));
}

static inline uint32_t sp805_read_wdog_pcell_id(unsigned long base, unsigned int id)
{
	assert(base);
	assert(id < 4);
	return mmio_read_32(base + SP805_WDOG_PCELL_ID_OFF + (id << 2));
}

void sp805_wdog_start(uint32_t wdog_cycles)
{
	/* Unlock to access the watchdog registers */
	sp805_write_wdog_lock(SP805_WDOG_BASE, SP805_WDOG_UNLOCK_ACCESS);

	/* Write the number of cycles needed */
	sp805_write_wdog_load(SP805_WDOG_BASE, wdog_cycles);

	/* Enable reset interrupt and watchdog interrupt on expiry */
	sp805_write_wdog_ctrl(SP805_WDOG_BASE,
			SP805_WDOG_CTRL_RESEN | SP805_WDOG_CTRL_INTEN);

	/* Lock registers so that they can't be accidently overwritten */
	sp805_write_wdog_lock(SP805_WDOG_BASE, 0x0);
}

void sp805_wdog_stop(void)
{
	/* Unlock to access the watchdog registers */
	sp805_write_wdog_lock(SP805_WDOG_BASE, SP805_WDOG_UNLOCK_ACCESS);

	/* Clearing INTEN bit stops the counter */
	sp805_write_wdog_ctrl(SP805_WDOG_BASE, 0x00);

	/* Lock registers so that they can't be accidently overwritten */
	sp805_write_wdog_lock(SP805_WDOG_BASE, 0x0);
}

void sp805_wdog_refresh(void)
{
	/* Unlock to access the watchdog registers */
	sp805_write_wdog_lock(SP805_WDOG_BASE, SP805_WDOG_UNLOCK_ACCESS);

	/*
	 * Write of any value to WdogIntClr clears interrupt and reloads
	 * the counter from the value in WdogLoad Register.
	 */
	sp805_write_wdog_int_clr(SP805_WDOG_BASE, 1);

	/* Lock registers so that they can't be accidently overwritten */
	sp805_write_wdog_lock(SP805_WDOG_BASE, 0x0);
}
