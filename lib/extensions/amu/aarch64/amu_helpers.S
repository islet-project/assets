/*
 * Copyright (c) 2017-2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <assert_macros.S>
#include <asm_macros.S>

	.globl	amu_group0_cnt_read_internal
	.globl	amu_group1_cnt_read_internal

/*
 * uint64_t amu_group0_cnt_read_internal(int idx);
 *
 * Given `idx`, read the corresponding AMU counter
 * and return it in `x0`.
 */
func amu_group0_cnt_read_internal
	adr	x1, 1f
#if ENABLE_ASSERTIONS
	/*
	 * It can be dangerous to call this function with an
	 * out of bounds index.  Ensure `idx` is valid.
	 */
	tst	x0, #~3
	ASM_ASSERT(eq)
#endif
	/*
	 * Given `idx` calculate address of mrs/ret instruction pair
	 * in the table below.
	 */
	add	x1, x1, x0, lsl #3	/* each mrs/ret sequence is 8 bytes */
#if ENABLE_BTI
	add	x1, x1, x0, lsl #2	/* + "bti j" instruction */
#endif
	br	x1

1:	read	AMEVCNTR00_EL0		/* index 0 */
	read	AMEVCNTR01_EL0		/* index 1 */
	read	AMEVCNTR02_EL0		/* index 2 */
	read	AMEVCNTR03_EL0		/* index 3 */
endfunc amu_group0_cnt_read_internal

/*
 * uint64_t amu_group1_cnt_read_internal(int idx);
 *
 * Given `idx`, read the corresponding AMU counter
 * and return it in `x0`.
 */
func amu_group1_cnt_read_internal
	adr	x1, 1f
#if ENABLE_ASSERTIONS
	/*
	 * It can be dangerous to call this function with an
	 * out of bounds index.  Ensure `idx` is valid.
	 */
	tst	x0, #~0xF
	ASM_ASSERT(eq)
#endif
	/*
	 * Given `idx` calculate address of mrs/ret instruction pair
	 * in the table below.
	 */
	add	x1, x1, x0, lsl #3	/* each mrs/ret sequence is 8 bytes */
#if ENABLE_BTI
	add	x1, x1, x0, lsl #2	/* + "bti j" instruction */
#endif
	br	x1

1:	read	AMEVCNTR10_EL0		/* index 0 */
	read	AMEVCNTR11_EL0		/* index 1 */
	read	AMEVCNTR12_EL0		/* index 2 */
	read	AMEVCNTR13_EL0		/* index 3 */
	read	AMEVCNTR14_EL0		/* index 4 */
	read	AMEVCNTR15_EL0		/* index 5 */
	read	AMEVCNTR16_EL0		/* index 6 */
	read	AMEVCNTR17_EL0		/* index 7 */
	read	AMEVCNTR18_EL0		/* index 8 */
	read	AMEVCNTR19_EL0		/* index 9 */
	read	AMEVCNTR1A_EL0		/* index 10 */
	read	AMEVCNTR1B_EL0		/* index 11 */
	read	AMEVCNTR1C_EL0		/* index 12 */
	read	AMEVCNTR1D_EL0		/* index 13 */
	read	AMEVCNTR1E_EL0		/* index 14 */
	read	AMEVCNTR1F_EL0		/* index 15 */
endfunc amu_group1_cnt_read_internal
