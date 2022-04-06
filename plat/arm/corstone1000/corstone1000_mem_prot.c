/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>
#include <psci.h>
#include <utils_def.h>
#include <xlat_tables_v2.h>

#define NS_IMAGE_OFFSET		TFTF_BASE
#define NS_IMAGE_LIMIT		(NS_IMAGE_OFFSET + (32 << TWO_MB_SHIFT))

static const mem_region_t corstone1000_ram_ranges[] = {
	{NS_IMAGE_LIMIT, 128 << TWO_MB_SHIFT},
};

const mem_region_t *plat_get_prot_regions(int *nelem)
{
	*nelem = ARRAY_SIZE(corstone1000_ram_ranges);
	return corstone1000_ram_ranges;
}
