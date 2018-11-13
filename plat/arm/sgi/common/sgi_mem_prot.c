/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>

#define SGI_DRAM1_NS_START	(TFTF_BASE + 0x4000000)
#define SGI_DRAM1_NS_SIZE	0x10000000

static const mem_region_t sgi_ram_ranges[] = {
	{ SGI_DRAM1_NS_START, SGI_DRAM1_NS_SIZE },
};

const mem_region_t *plat_get_prot_regions(int *nelem)
{
	*nelem = ARRAY_SIZE(sgi_ram_ranges);
	return sgi_ram_ranges;
}
