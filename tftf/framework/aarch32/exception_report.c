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

/* We save r0-r12. */
#define GPREGS_CNT 13

/* Set of registers saved by the crash_dump() assembly function. */
struct cpu_context {
	u_register_t regs[GPREGS_CNT];
	u_register_t lr;
	u_register_t sp;
};

void __dead2 print_exception(const struct cpu_context *ctx)
{
	u_register_t mpid = read_mpidr();

	/*
	 * The instruction barrier ensures we don't read stale values of system
	 * registers.
	 */
	isb();

	printf("Unhandled exception on CPU%u.\n", platform_get_core_pos(mpid));

	/* Dump some interesting system registers. */
	printf("System registers:\n");
	printf("  MPIDR=0x%lx\n", mpid);
	printf("  HSR=0x%lx  ELR=0x%lx  SPSR=0x%lx\n", read_hsr(),
	       read_elr_hyp(), read_spsr());

	/* Dump general-purpose registers. */
	printf("General-purpose registers:\n");
	for (int i = 0; i < GPREGS_CNT; ++i) {
		printf("  r%u=0x%lx\n", i, ctx->regs[i]);
	}
	printf("  LR=0x%lx\n", ctx->lr);
	printf("  SP=0x%lx\n", ctx->sp);

	while (1)
		wfi();
}
