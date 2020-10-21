/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>

#define TC0_DRAM1_NS_START	(TFTF_BASE + 0x4000000)
#define TC0_DRAM1_NS_SIZE	0x10000000

static const mem_region_t tc0_ram_ranges[] = {
	{ TC0_DRAM1_NS_START, TC0_DRAM1_NS_SIZE }
};

const mem_region_t *plat_get_prot_regions(int *nelem)
{
	*nelem = ARRAY_SIZE(tc0_ram_ranges);
	return tc0_ram_ranges;
}
