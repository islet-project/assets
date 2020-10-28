/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <mmio.h>
#include <platform.h>
#include <stddef.h>

#include <utils_def.h>

#define WAKE_AOWAKE_RTC_ID				U(73)
#define WAKE_AOWAKE_CNTRL_73				U(0x124)
#define WAKE_AOWAKE_MASK_W_73				U(0x2A4)
#define WAKE_AOWAKE_STATUS_W_73				U(0x430)
#define WAKE_AOWAKE_TIER2_CTRL_0			U(0x4B0)
#define WAKE_AOWAKE_TIER2_ROUTING_31_0_0		U(0x4CC)
#define WAKE_AOWAKE_TIER2_ROUTING_63_32_0		U(0x4D0)
#define WAKE_AOWAKE_TIER2_ROUTING_95_64_0		U(0x4D4)


#define WAKE_AOWAKE_TIER2_CTRL_0_INT_EN_TRUE		BIT_32(0)
#define WAKE_AOWAKE_CNTRL_73_COAL_EN_FIELD		BIT_32(6)
#define WAKE_AOWAKE_CNTRL_73_COAL_GRP_SEL_FIELD		BIT_32(5)
#define WAKE_AOWAKE_CNTRL_73_LEVEL_FIELD		BIT_32(3)
#define WAKE_AOWAKE_STATUS_W_73_CLEAR_FALSE		U(0)
#define WAKE_AOWAKE_MASK_W_73_MASK_UNMASK		U(1)

static inline void aowake_write_32(uint32_t offset, uint32_t value)
{
	mmio_write_32(TEGRA_AOWAKE_BASE + offset, value);
}

void tegra_set_rtc_as_wakeup_source(void)
{
	/*
	 * Configure RTC as the wake source to tier2 = CCPLEX,
	 * and disable others
	 */
	aowake_write_32(WAKE_AOWAKE_TIER2_ROUTING_31_0_0, 0U);
	aowake_write_32(WAKE_AOWAKE_TIER2_ROUTING_63_32_0, 0U);
	aowake_write_32(WAKE_AOWAKE_TIER2_ROUTING_95_64_0,
			BIT_32(WAKE_AOWAKE_RTC_ID - 64));

	/* Enable the tier 2 wake up */
	aowake_write_32(WAKE_AOWAKE_TIER2_CTRL_0, WAKE_AOWAKE_TIER2_CTRL_0_INT_EN_TRUE);

	/* Configure the RTC wake up source as per the golden register value */
	aowake_write_32(WAKE_AOWAKE_CNTRL_73, WAKE_AOWAKE_CNTRL_73_COAL_EN_FIELD |
			WAKE_AOWAKE_CNTRL_73_COAL_GRP_SEL_FIELD |
			WAKE_AOWAKE_CNTRL_73_LEVEL_FIELD);

	/* Clear current wake status of RTC then enable it as a wake source */
	aowake_write_32(WAKE_AOWAKE_STATUS_W_73, WAKE_AOWAKE_STATUS_W_73_CLEAR_FALSE);
	aowake_write_32(WAKE_AOWAKE_MASK_W_73, WAKE_AOWAKE_MASK_W_73_MASK_UNMASK);
}
