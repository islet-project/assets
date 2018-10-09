/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <mmio.h>
#include <platform_def.h>
#include <stdint.h>
#include <stdlib.h>

uintptr_t bound_rand(uintptr_t min, uintptr_t max)
{
	/*
	 * This is not ideal as some numbers will never be generated because of
	 * the integer arithmetic rounding.
	 */
	return ((rand() * (UINT64_MAX/RAND_MAX)) % (max - min)) + min;
}

/*******************************************************************************
 * Test framework helpers
 ******************************************************************************/

void expect(int expr, int expected)
{
	if (expr != expected) {
		ERROR("Expected value %i, got %i\n", expected, expr);
		while (1)
			continue;
	}
}

void announce_test_section_start(const char *test_sect_desc)
{
	INFO("========================================\n");
	INFO("Starting %s tests\n", test_sect_desc);
	INFO("========================================\n");
}
void announce_test_section_end(const char *test_sect_desc)
{
	INFO("========================================\n");
	INFO("End of %s tests\n", test_sect_desc);
	INFO("========================================\n");
}

void announce_test_start(const char *test_desc)
{
	INFO("[+] %s\n", test_desc);
}

void announce_test_end(const char *test_desc)
{
	INFO("Test \"%s\" passed.\n", test_desc);
}

void sp_sleep(uint32_t duration_sec)
{
	uint32_t timer_freq = mmio_read_32(SYS_CNT_CONTROL_BASE + CNTFID_OFF);
	VERBOSE("%s: Timer frequency = %u\n", __func__, timer_freq);

	INFO("%s: Sleeping for %u seconds...\n", __func__, duration_sec);
	uint64_t time1 = mmio_read_64(SYS_CNT_READ_BASE);
	volatile uint64_t time2 = time1;
	while ((time2 - time1) < duration_sec * timer_freq) {
		time2 = mmio_read_64(SYS_CNT_READ_BASE);
	}

	INFO("%s: Done\n", __func__);
}
