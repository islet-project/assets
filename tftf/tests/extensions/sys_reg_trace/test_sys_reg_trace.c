/*
 * Copyright (c) 2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <test_helpers.h>
#include <tftf_lib.h>
#include <tftf.h>

#include "./test_sys_reg_trace.h"

static uint32_t get_trace_arch_ver(void)
{
	uint32_t val = read_trcdevarch();
	val = (val >> TRCDEVARCH_ARCHVER_SHIFT) & TRCDEVARCH_ARCHVER_MASK;

	return val;
}

/*
 * EL3 is expected to allow access to trace system registers from EL2.
 * Reading these register will trap to EL3 and crash when EL3 has not
 * allowed access.
 */
test_result_t test_sys_reg_trace_enabled(void)
{
	SKIP_TEST_IF_SYS_REG_TRACE_NOT_SUPPORTED();

	/*
	 * Read few ETMv4 system trace registers to verify correct access
	 * been provided from EL3.
	 */
	uint32_t trace_arch_ver __unused = get_trace_arch_ver();
	read_trcauxctlr();
	read_trcccctlr();
	read_trcbbctlr();
	read_trcclaimset();
	read_trcclaimclr();

	/*
	 * Read few ETE system trace registers to verify correct access
	 * been provided from EL3. ETE system trace register access are
	 * not possible from NS-EL2 in aarch32 state.
	 */
#if __aarch64__
	if (trace_arch_ver == TRCDEVARCH_ARCHVER_ETE) {
		read_trcrsr();
		read_trcextinselr0();
		read_trcextinselr1();
		read_trcextinselr2();
		read_trcextinselr3();
	}
#endif /* __aarch64__ */

	return TEST_RESULT_SUCCESS;
}
