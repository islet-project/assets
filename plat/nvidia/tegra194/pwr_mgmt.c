/*
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <debug.h>
#include <mmio.h>
#include <platform.h>
#include <stddef.h>

#include <utils_def.h>

/*******************************************************************************
 * Memory Controller SMMU Bypass config register
 ******************************************************************************/
#define MC_SMMU_BYPASS_CONFIG		U(0x1820)

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
		.reg = TEGRA194_MC_BASE + MC_SMMU_BYPASS_CONFIG, \
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

void tegra194_pwr_mgmt_setup(void)
{
	/* index of END_OF_TABLE */
	tegra194_mc_context[0].val = ARRAY_SIZE(tegra194_mc_context) - 1U;

	/* save SMMU context for SC7-RF to restore */
	mmio_write_32(TEGRA194_SCRATCH_BASE + SCRATCH_SECURE_RSV73_SCRATCH,
		      ((uintptr_t)tegra194_mc_context) >> 12);
}
