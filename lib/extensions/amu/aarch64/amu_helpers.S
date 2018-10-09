/*
 * Copyright (c) 2017, Arm Limited. All rights reserved.
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
#if ENABLE_ASSERTIONS
	/*
	 * It can be dangerous to call this function with an
	 * out of bounds index.  Ensure `idx` is valid.
	 */
	mov	x1, x0
	lsr	x1, x1, #2
	cmp	x1, #0
	ASM_ASSERT(eq)
#endif

	/*
	 * Given `idx` calculate address of mrs/ret instruction pair
	 * in the table below.
	 */
	adr	x1, 1f
	lsl	x0, x0, #3		/* each mrs/ret sequence is 8 bytes */
	add	x1, x1, x0
	br	x1

1:
	mrs	x0, AMEVCNTR00_EL0	/* index 0 */
	ret
	mrs	x0, AMEVCNTR01_EL0	/* index 1 */
	ret
	mrs	x0, AMEVCNTR02_EL0	/* index 2 */
	ret
	mrs	x0, AMEVCNTR03_EL0	/* index 3 */
	ret
endfunc amu_group0_cnt_read_internal

/*
 * uint64_t amu_group1_cnt_read_internal(int idx);
 *
 * Given `idx`, read the corresponding AMU counter
 * and return it in `x0`.
 */
func amu_group1_cnt_read_internal
#if ENABLE_ASSERTIONS
	/*
	 * It can be dangerous to call this function with an
	 * out of bounds index.  Ensure `idx` is valid.
	 */
	mov	x1, x0
	lsr	x1, x1, #4
	cmp	x1, #0
	ASM_ASSERT(eq)
#endif

	/*
	 * Given `idx` calculate address of mrs/ret instruction pair
	 * in the table below.
	 */
	adr	x1, 1f
	lsl	x0, x0, #3		/* each mrs/ret sequence is 8 bytes */
	add	x1, x1, x0
	br	x1

1:
	mrs	x0, AMEVCNTR10_EL0	/* index 0 */
	ret
	mrs	x0, AMEVCNTR11_EL0	/* index 1 */
	ret
	mrs	x0, AMEVCNTR12_EL0	/* index 2 */
	ret
	mrs	x0, AMEVCNTR13_EL0	/* index 3 */
	ret
	mrs	x0, AMEVCNTR14_EL0	/* index 4 */
	ret
	mrs	x0, AMEVCNTR15_EL0	/* index 5 */
	ret
	mrs	x0, AMEVCNTR16_EL0	/* index 6 */
	ret
	mrs	x0, AMEVCNTR17_EL0	/* index 7 */
	ret
	mrs	x0, AMEVCNTR18_EL0	/* index 8 */
	ret
	mrs	x0, AMEVCNTR19_EL0	/* index 9 */
	ret
	mrs	x0, AMEVCNTR1A_EL0	/* index 10 */
	ret
	mrs	x0, AMEVCNTR1B_EL0	/* index 11 */
	ret
	mrs	x0, AMEVCNTR1C_EL0	/* index 12 */
	ret
	mrs	x0, AMEVCNTR1D_EL0	/* index 13 */
	ret
	mrs	x0, AMEVCNTR1E_EL0	/* index 14 */
	ret
	mrs	x0, AMEVCNTR1F_EL0	/* index 15 */
	ret
endfunc amu_group1_cnt_read_internal
