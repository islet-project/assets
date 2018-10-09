/*
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AMU_H__
#define __AMU_H__

#include <platform_def.h>
#include <stdint.h>

#define AMU_GROUP0_NR_COUNTERS		4
#define AMU_GROUP0_COUNTERS_MASK	0xf

#ifdef PLAT_AMU_GROUP1_COUNTERS_MASK
#define AMU_GROUP1_COUNTERS_MASK	PLAT_AMU_GROUP1_COUNTERS_MASK
#else
#define AMU_GROUP1_COUNTERS_MASK	0
#endif

#ifdef PLAT_AMU_GROUP1_NR_COUNTERS
#define AMU_GROUP1_NR_COUNTERS		PLAT_AMU_GROUP1_NR_COUNTERS
#else
#define AMU_GROUP1_NR_COUNTERS		0
#endif

#define AMU_GROUP0_MAX_NR_COUNTERS	4
#define AMU_GROUP1_MAX_NR_COUNTERS	16

int amu_supported(void);
uint64_t amu_group0_cnt_read(int idx);
uint64_t amu_group1_cnt_read(int idx);

#endif /* __AMU_H__ */
