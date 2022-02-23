/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

#include <sgi_soc_platform_def_v2.h>

/*
 * The RD-N2 Cfg1 platform is a variant of the RD-N2 platform with a
 * reduced interconnect mesh size (3x3) and core count (8-cores).
 *
 * The $CSS_SGI_PLATFORM_VARIANT flag is set to 1 for RD-N2-Cfg1 platform.
 */
#if (CSS_SGI_PLATFORM_VARIANT == 1)
#define PLAT_ARM_CLUSTER_COUNT		U(8)
#else
#define PLAT_ARM_CLUSTER_COUNT		U(16)
#endif
#define CSS_SGI_MAX_CPUS_PER_CLUSTER	U(1)
#define CSS_SGI_MAX_PE_PER_CPU		U(1)

/* GIC-600 & interrupt handling related constants */
#define PLAT_ARM_GICD_BASE		UL(0x30000000)
#if (CSS_SGI_PLATFORM_VARIANT == 1)
#define PLAT_ARM_GICR_BASE		UL(0x30100000)
#else
#define PLAT_ARM_GICR_BASE		UL(0x301C0000)
#endif
#define PLAT_ARM_GICC_BASE		UL(0x2C000000)

/* Platform specific page table and MMU setup constants */
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ULL << 46)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ULL << 46)

#endif /* PLATFORM_DEF_H */
