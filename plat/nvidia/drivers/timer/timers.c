/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stddef.h>
#include <platform.h>

#include <mmio.h>
#include <timer.h>
#include <tftf_lib.h>
#include <utils_def.h>

/* timer granularity in ms */
#define TEGRA_RTC_STEP_VALUE_MS			U(5)

/* IRQ value for Tegra Timer0 */
#define TEGRA_RTC_IRQ				U(42)

/* set to 1 = busy every eight 32kHz clocks during copy of sec+msec to AHB */
#define TEGRA_RTC_REG_BUSY			U(0x004)
#define TEGRA_RTC_REG_SECONDS			U(0x008)
/* when msec is read, the seconds are buffered into shadow seconds. */
#define TEGRA_RTC_REG_SHADOW_SECONDS		U(0x00c)
#define TEGRA_RTC_REG_MILLI_SECONDS		U(0x010)
#define TEGRA_RTC_REG_SECONDS_ALARM0		U(0x014)
#define TEGRA_RTC_REG_SECONDS_ALARM1		U(0x018)
#define TEGRA_RTC_REG_MILLI_SECONDS_ALARM0	U(0x01c)
#define TEGRA_RTC_REG_MSEC_CDN_ALARM0		U(0x024)
#define TEGRA_RTC_REG_INTR_MASK			U(0x028)
/* write 1 bits to clear status bits */
#define TEGRA_RTC_REG_INTR_STATUS		U(0x02c)

/*
 * bits in the TEGRA_RTC_REG_BUSY register
 * bit 0: 1 = busy, 0 = idle
 */
#define TEGRA_RTC_REG_BUSY_BIT			BIT_32(0)

/* bits in INTR_MASK and INTR_STATUS */
#define TEGRA_RTC_INTR_MSEC_CDN_ALARM		BIT_32(4)
#define TEGRA_RTC_INTR_SEC_CDN_ALARM		BIT_32(3)
#define TEGRA_RTC_INTR_MSEC_ALARM		BIT_32(2)
#define TEGRA_RTC_INTR_SEC_ALARM1		BIT_32(1)
#define TEGRA_RTC_INTR_SEC_ALARM0		BIT_32(0)

static bool is_rtc_busy(void)
{
	uint32_t reg = mmio_read_32(TEGRA_RTC_BASE + TEGRA_RTC_REG_BUSY) &
			TEGRA_RTC_REG_BUSY_BIT;

	/* 1 = busy, 0 = idle */
	return (reg == 1U);
}

/*
 * Wait for hardware to be ready for writing.
 * This function tries to maximize the amount of time before the next update.
 * It does this by waiting for the RTC to become busy with its periodic update,
 * then returning once the RTC first becomes not busy.
 * This periodic update (where the seconds and milliseconds are copied to the
 * AHB side) occurs every eight 32kHz clocks (~250uS).
 * The behavior of this function allows us to make some assumptions without
 * introducing a race, because 250uS is plenty of time to write a value.
 */
static void wait_until_idle(void)
{
	uint32_t retries = 500U;

	/* wait until idle */
	while (is_rtc_busy() || (retries-- > 0U)) {
		waitus(1ULL);
	}
}

static void timer_idle_write_32(uint32_t offset, uint32_t val)
{
	/* wait until the RTC is idle first */
	wait_until_idle();

	/* actual write */
	mmio_write_32(TEGRA_RTC_BASE + offset, val);

	/* wait until RTC has processed the write */
	wait_until_idle();
}

static uint32_t timer_idle_read_32(uint32_t offset)
{
	/* wait until the RTC is idle first */
	wait_until_idle();

	/* actual read */
	return mmio_read_32(TEGRA_RTC_BASE + offset);
}

static int cancel_timer(void)
{
	/* read current values to clear them */
	(void)timer_idle_read_32(TEGRA_RTC_REG_MILLI_SECONDS);
	(void)timer_idle_read_32(TEGRA_RTC_REG_SHADOW_SECONDS);

	/* clear the alarm */
	timer_idle_write_32(TEGRA_RTC_REG_MSEC_CDN_ALARM0, 0U);
	/* clear all status values */
	timer_idle_write_32(TEGRA_RTC_REG_INTR_STATUS, 0xffffffffU);
	/* disable all interrupts */
	timer_idle_write_32(TEGRA_RTC_REG_INTR_MASK, 0U);

	return 0;
}

static int program_timer(unsigned long time_out_ms)
{
	uint32_t reg;

	/* set timer value */
	reg = BIT_32(31) | (0x0fffffffU & time_out_ms);
	timer_idle_write_32(TEGRA_RTC_REG_MSEC_CDN_ALARM0, reg);

	/* enable timer interrupt */
	timer_idle_write_32(TEGRA_RTC_REG_INTR_MASK, TEGRA_RTC_INTR_MSEC_ALARM);

	/* program timeout value */
	reg = timer_idle_read_32(TEGRA_RTC_REG_MILLI_SECONDS);
	timer_idle_write_32(TEGRA_RTC_REG_MILLI_SECONDS_ALARM0, reg + time_out_ms);

	return 0;
}

static int handler_timer(void)
{
	uint32_t __unused reg, status, mask;

	/* disable timer interrupt */
	reg = timer_idle_read_32(TEGRA_RTC_REG_INTR_MASK);
	reg &= ~TEGRA_RTC_INTR_MSEC_CDN_ALARM;
	timer_idle_write_32(TEGRA_RTC_REG_INTR_MASK, reg);

	/* read current values to clear them */
	reg = timer_idle_read_32(TEGRA_RTC_REG_MILLI_SECONDS);
	reg = timer_idle_read_32(TEGRA_RTC_REG_SHADOW_SECONDS);

	/* clear interrupts */
	status = timer_idle_read_32(TEGRA_RTC_REG_INTR_STATUS);
	mask = timer_idle_read_32(TEGRA_RTC_REG_INTR_MASK);
	mask &= ~status;
	if (status != 0U) {
		timer_idle_write_32(TEGRA_RTC_REG_INTR_MASK, mask);
		timer_idle_write_32(TEGRA_RTC_REG_INTR_STATUS, status);
	}

	return 0;
}

static const plat_timer_t tegra_timers = {
	.program = program_timer,
	.cancel = cancel_timer,
	.handler = handler_timer,
	.timer_step_value = TEGRA_RTC_STEP_VALUE_MS,
	.timer_irq = TEGRA_RTC_IRQ
};

int plat_initialise_timer_ops(const plat_timer_t **timer_ops)
{
	assert(timer_ops != NULL);
	*timer_ops = &tegra_timers;

	/* clear the timers */
	cancel_timer();

	return 0;
}
