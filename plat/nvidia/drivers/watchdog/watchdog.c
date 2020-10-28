/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <mmio.h>
#include <platform.h>

/* Timer registers */
#define TIMER_PTV			U(0)
 #define TIMER_EN_BIT			BIT_32(31)
 #define TIMER_PERIODIC_BIT		BIT_32(30)
#define TIMER_PCR			U(0x4)
 #define TIMER_PCR_INTR_BIT		BIT_32(30)

/* WDT registers */
#define WDT_CFG				U(0)
 #define WDT_CFG_TMR_SRC		U(0)		/* for TMR0. */
 #define WDT_CFG_PERIOD_BIT		BIT_32(4)
 #define WDT_CFG_INT_EN_BIT		BIT_32(12)
 #define WDT_CFG_SYS_RST_EN_BIT		BIT_32(14)
 #define WDT_CFG_PMC2CAR_RST_EN_BIT	BIT_32(15)
#define WDT_CMD				U(8)
 #define WDT_CMD_START_COUNTER_BIT	BIT_32(0)
 #define WDT_CMD_DISABLE_COUNTER_BIT	BIT_32(1)
#define WDT_UNLOCK			U(0xC)
 #define WDT_UNLOCK_PATTERN		U(0xC45A)

/* watchdog will fire after this timeout value is reached */
#define WDT_TIMEOUT_SECONDS		U(10)
#define WDT_TIMEOUT_MULTIPLIER		UL(125000)

static inline void tegra_wdt_write(uint32_t offset, uint32_t val)
{
	mmio_write_32(TEGRA_WDT0_BASE + offset, val);
}

static inline uint32_t tegra_wdt_read(uint32_t offset)
{
	return mmio_read_32(TEGRA_WDT0_BASE + offset);
}

static inline void tegra_tmr_write(uint32_t offset, uint32_t val)
{
	mmio_write_32(TEGRA_TMR0_BASE + offset, val);
}

static inline uint32_t tegra_tmr_read(uint32_t offset)
{
	return mmio_read_32(TEGRA_TMR0_BASE + offset);
}

/*
 * Start the watchdog timer
 */
void tftf_platform_watchdog_set(void)
{
	uint32_t val;

	/* Clear pending interrupts first */
	tegra_tmr_write(TIMER_PCR, TIMER_PCR_INTR_BIT);

	/*
	 * Normally, we would set the period to 1 second by writing 125000ul,
	 * but the watchdog system reset actually occurs on the 4th expiration
	 * of this counter, so we set the period to 1/4 of this amount.
	 */
	val = (WDT_TIMEOUT_SECONDS * WDT_TIMEOUT_MULTIPLIER) / 4U;
	val |= (TIMER_EN_BIT | TIMER_PERIODIC_BIT);
	tegra_tmr_write(TIMER_PTV, val);

	/*
	 * Set number of periods and start counter.
	 */
	val = WDT_CFG_TMR_SRC | WDT_CFG_SYS_RST_EN_BIT |
		WDT_CFG_PMC2CAR_RST_EN_BIT;
	tegra_wdt_write(WDT_CFG, val);
	tegra_wdt_write(WDT_CMD, WDT_CMD_START_COUNTER_BIT);
}

/*
 * Stop the watchdog timer
 */
void tftf_platform_watchdog_reset(void)
{
	tegra_tmr_write(TIMER_PCR, TIMER_PCR_INTR_BIT);
	tegra_wdt_write(WDT_UNLOCK, WDT_UNLOCK_PATTERN);
	tegra_wdt_write(WDT_CMD, WDT_CMD_DISABLE_COUNTER_BIT);
	tegra_tmr_write(TIMER_PTV, 0U);
}
