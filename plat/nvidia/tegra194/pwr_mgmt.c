/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <debug.h>
#include <mmio.h>
#include <platform.h>
#include <stddef.h>

#include <utils_def.h>

/*******************************************************************************
 * Secure Scratch 73 to save base address of SMMU register context
 ******************************************************************************/
#define SCRATCH_SECURE_RSV73_SCRATCH	U(0x2ac)

typedef struct mc_regs {
	uint32_t reg;
	uint32_t val;
} mc_regs_t;

#define mc_smmu_bypass_cfg \
	{ \
		.reg = TEGRA_SMMU0_BASE, \
		.val = 0x00000000U, \
	}

#define START_OF_TABLE \
	{ \
		.reg = 0xCAFE05C7U, \
		.val = 0x00000000U, \
	}

#define END_OF_TABLE \
	{ \
		.reg = 0xFFFFFFFFU, \
		.val = 0xFFFFFFFFU, \
	}

/*******************************************************************************
 * Array to hold MC context for Tegra194
 ******************************************************************************/
static __attribute__((aligned(16))) mc_regs_t tegra194_mc_context[] = {
	START_OF_TABLE,
	mc_smmu_bypass_cfg,	/* TBU settings */
	END_OF_TABLE,
};

void tegra_pwr_mgmt_setup(void)
{
	uintptr_t smmu_ctx_base = (uintptr_t)TEGRA_SMMU_CTX_BASE;

	/* index of END_OF_TABLE */
	tegra194_mc_context[0].val = ARRAY_SIZE(tegra194_mc_context) - 1U;

	/* prepare dummy context */
	for (unsigned int i = 1U; i < ARRAY_SIZE(tegra194_mc_context) - 1U; i++) {
		tegra194_mc_context[i].val = mmio_read_32(tegra194_mc_context[i].reg);
	}

	/* save context for the SC7-RF */
	memcpy((void *)smmu_ctx_base, (void *)tegra194_mc_context,
		sizeof(tegra194_mc_context));
	flush_dcache_range(smmu_ctx_base, sizeof(tegra194_mc_context));

	/* save SMMU context for SC7-RF to restore */
	mmio_write_32(TEGRA_SCRATCH_BASE + SCRATCH_SECURE_RSV73_SCRATCH,
		      smmu_ctx_base >> 12);
}
