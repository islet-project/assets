/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <debug.h>
#include <sp_helpers.h>
#include <types.h>

#include "cactus.h"

extern uintptr_t __TEXT_START__;

void system_setup_tests(void)
{
	const char *test_sect_desc = "system setup";

	announce_test_section_start(test_sect_desc);

	/*
	 * Try accessing CTR_EL0 register. This should work if SCTLR_EL1.UCT bit
	 * has been correctly setup by TF.
	 */
	const char *test_desc1 = "Read CTR_EL0 register";

	announce_test_start(test_desc1);

	uint32_t ctr __unused = read_ctr_el0();

	INFO("CTR_EL0 = 0x%x\n", ctr);
	announce_test_end(test_desc1);

	/*
	 * Try to execute a cache maintenance instruction. This should work if
	 * SCTLR_EL1.UCI bit has been correctly setup by TF.
	 */
	const char *test_desc2 = "Access to cache maintenance operations";

	announce_test_start(test_desc2);
	flush_dcache_range((uintptr_t)&__TEXT_START__, 1);
	announce_test_end(test_desc2);

	/*
	 * Try accessing a floating point register. This should not trap to
	 * S-EL1.
	 */
	const char *test_desc3 = "Access to FP regs";

	announce_test_start(test_desc3);
	/*
	 * Can't use the 'double' type here because Cactus (like the rest of
	 * the TF code) is compiled with GCC's -mgeneral-regs-only compiler flag
	 * that disables floating point support in GCC.
	 */
	uint64_t fp_reg;

	__asm__ volatile("fmov %0, d0" : "=r" (fp_reg) :: "d0");
	INFO("D0 = 0x%llx\n", fp_reg);
	__asm__ volatile(
		"fmov d0, #1.0 \n\t"
		"fmov %0, d0 \n\t"
		: "=r" (fp_reg)
		:
		: "d0");
	INFO("D0 = 0x%llx\n", fp_reg);
	announce_test_end(test_desc3);

	announce_test_section_end(test_sect_desc);
}
