/*
 * Copyright (c) 2018-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

#include <sgi_soc_platform_def.h>

#define PLAT_ARM_CLUSTER_COUNT		U(2)
#define CSS_SGI_MAX_CPUS_PER_CLUSTER	U(4)
#define CSS_SGI_MAX_PE_PER_CPU		U(1)

/* GIC related constants */
#define PLAT_ARM_GICD_BASE		UL(0x30000000)
#define PLAT_ARM_GICC_BASE		UL(0x2C000000)
#define PLAT_ARM_GICR_BASE		UL(0x300C0000)

/* Platform specific page table and MMU setup constants */
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ULL << 32)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ULL << 32)

#endif /* PLATFORM_DEF_H */
