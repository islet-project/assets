/*
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AMU_PRIVATE_H__
#define __AMU_PRIVATE_H__

#include <stdint.h>

uint64_t amu_group0_cnt_read_internal(int idx);
uint64_t amu_group1_cnt_read_internal(int idx);

#endif /* __AMU_PRIVATE_H__ */
