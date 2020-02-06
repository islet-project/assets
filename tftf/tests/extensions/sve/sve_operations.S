/*
 * Copyright (c) 2019-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

#include "./test_sve.h"

#ifdef __aarch64__
#if __GNUC__ > 8 || (__GNUC__ == 8 && __GNUC_MINOR__ > 0)

/*
 * Based on example code from the Arm Compiler Scalable Vector Extension User
 * Guide[1].
 * [1] https://developer.arm.com/docs/100891/latest/getting-started-with-the-sve-compiler/compiling-c-and-c-code-for-sve-enabled-targets
 */

	.arch armv8.2-a+crc+fp16+sve
	.global	sve_subtract_arrays
func sve_subtract_arrays
	mov	x4, SVE_ARRAYSIZE
	mov	x5, x4
	mov	x3, 0
	whilelo	p0.s, xzr, x4
.loop:
	ld1w	z0.s, p0/z, [x1, x3, lsl 2]
	ld1w	z1.s, p0/z, [x2, x3, lsl 2]
	sub	z0.s, z0.s, z1.s
	st1w	z0.s, p0, [x0, x3, lsl 2]
	incw	x3
	whilelo	p0.s, x3, x5
	bne	.loop
	ret
endfunc sve_subtract_arrays

#endif /* __GNUC__ > 8 || (__GNUC__ == 8 && __GNUC_MINOR__ > 0) */
#endif /* __aarch64__ */
