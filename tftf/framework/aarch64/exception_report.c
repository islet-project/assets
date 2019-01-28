/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stdio.h>

#include <arch_helpers.h>
#include <debug.h>
#include <platform.h>
#include <utils_def.h>

/* We save x0-x30. */
#define GPREGS_CNT 31

/* Set of registers saved by the crash_dump() assembly function. */
struct cpu_context {
	u_register_t regs[GPREGS_CNT];
	u_register_t sp;
};

/*
 * Read the EL1 or EL2 version of a register, depending on the current exception
 * level.
 */
#define read_sysreg(_name)			\
	(IS_IN_EL2() ? read_##_name##_el2() : read_##_name##_el1())

void __dead2 print_exception(const struct cpu_context *ctx)
{
	u_register_t mpid = read_mpidr_el1();

	/*
	 * The instruction barrier ensures we don't read stale values of system
	 * registers.
	 */
	isb();

	printf("Unhandled exception on CPU%u.\n", platform_get_core_pos(mpid));

	/* Dump some interesting system registers. */
	printf("System registers:\n");
	printf("  MPIDR=0x%lx\n", mpid);
	printf("  ESR=0x%lx  ELR=0x%lx  FAR=0x%lx\n", read_sysreg(esr),
	       read_sysreg(elr), read_sysreg(far));
	printf("  SCTLR=0x%lx  SPSR=0x%lx  DAIF=0x%lx\n",
	       read_sysreg(sctlr), read_sysreg(spsr), read_daif());

	/* Dump general-purpose registers. */
	printf("General-purpose registers:\n");
	for (int i = 0; i < GPREGS_CNT; ++i) {
		printf("  x%u=0x%lx\n", i, ctx->regs[i]);
	}
	printf("  SP=0x%lx\n", ctx->sp);

	while (1)
		wfi();
}
