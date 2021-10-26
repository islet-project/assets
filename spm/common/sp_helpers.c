/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <mmio.h>
#include <platform_def.h>
#include <stdint.h>
#include <stdlib.h>
#include <ffa_svc.h>

#include "sp_helpers.h"

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
	INFO("Test \"%s\" end.\n", test_desc);
}

uint64_t sp_sleep_elapsed_time(uint32_t ms)
{
	uint64_t timer_freq = read_cntfrq_el0();

	VERBOSE("%s: Timer frequency = %llu\n", __func__, timer_freq);

	VERBOSE("%s: Sleeping for %u milliseconds...\n", __func__, ms);

	uint64_t time1 = virtualcounter_read();
	volatile uint64_t time2 = time1;

	while ((time2 - time1) < ((ms * timer_freq) / 1000U)) {
		time2 = virtualcounter_read();
	}

	return ((time2 - time1) * 1000) / timer_freq;
}

void sp_sleep(uint32_t ms)
{
	(void)sp_sleep_elapsed_time(ms);
}
