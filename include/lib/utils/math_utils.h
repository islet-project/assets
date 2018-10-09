/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MATH_UTILS_H__
#define __MATH_UTILS_H__

#include <stdint.h>

/* Simple utility to calculate `power` of a `base` number. */
static inline unsigned int pow(unsigned int base, unsigned int power)
{
	unsigned int result = 1;
	while (power) {
		result *= base;
		power--;
	}
	return result;
}

#endif /* __MATH_UTILS_H__ */
