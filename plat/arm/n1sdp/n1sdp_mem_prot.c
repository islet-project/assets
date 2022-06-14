/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform.h>

#define N1SDP_DRAM1_NS_START	(TFTF_BASE + 0x4000000)
#define N1SDP_DRAM1_NS_SIZE	0x10000000

static const mem_region_t n1sdp_ram_ranges[] = {
	{ N1SDP_DRAM1_NS_START, N1SDP_DRAM1_NS_SIZE }
};

const mem_region_t *plat_get_prot_regions(int *nelem)
{
	*nelem = ARRAY_SIZE(n1sdp_ram_ranges);
	return n1sdp_ram_ranges;
}
