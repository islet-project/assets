/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>
#include <psci.h>
#include <utils_def.h>
#include <xlat_tables_v2.h>

#define NS_IMAGE_OFFSET		TFTF_BASE
#define NS_IMAGE_LIMIT		(NS_IMAGE_OFFSET + (32 << TWO_MB_SHIFT))

static const mem_region_t juno_ram_ranges[] = {
	{NS_IMAGE_LIMIT, 128 << TWO_MB_SHIFT},
#ifdef AARCH64
	{JUNO_DRAM2_BASE, 1 << ONE_GB_SHIFT},
#endif
};

const mem_region_t *plat_get_prot_regions(int *nelem)
{
	*nelem = ARRAY_SIZE(juno_ram_ranges);
	return juno_ram_ranges;
}
