/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
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
 * and return it in `r0`.
 */
func amu_group0_cnt_read_internal
#if ENABLE_ASSERTIONS
	/* `idx` should be between [0, 3] */
	mov	r1, r0
	lsr	r1, r1, #2
	cmp	r1, #0
	ASM_ASSERT(eq)
#endif

	/*
	 * Given `idx` calculate address of ldcopr16/bx lr instruction pair
	 * in the table below.
	 */
	adr	r1, 1f
	lsl	r0, r0, #3	/* each ldcopr16/bx lr sequence is 8 bytes */
	add	r1, r1, r0
	bx	r1
1:
	ldcopr16	r0, r1, AMEVCNTR00	/* index 0 */
	bx		lr
	ldcopr16	r0, r1, AMEVCNTR01	/* index 1 */
	bx 		lr
	ldcopr16	r0, r1, AMEVCNTR02	/* index 2 */
	bx 		lr
	ldcopr16	r0, r1, AMEVCNTR03	/* index 3 */
	bx 		lr
endfunc amu_group0_cnt_read_internal

/*
 * uint64_t amu_group1_cnt_read_internal(int idx);
 *
 * Given `idx`, read the corresponding AMU counter
 * and return it in `r0`.
 */
func amu_group1_cnt_read_internal
#if ENABLE_ASSERTIONS
	/* `idx` should be between [0, 15] */
	mov	r1, r0
	lsr	r1, r1, #4
	cmp	r1, #0
	ASM_ASSERT(eq)
#endif

	/*
	 * Given `idx` calculate address of ldcopr16/bx lr instruction pair
	 * in the table below.
	 */
	adr	r1, 1f
	lsl	r0, r0, #3	/* each ldcopr16/bx lr sequence is 8 bytes */
	add	r1, r1, r0
	bx	r1

1:
	ldcopr16	r0,r1, AMEVCNTR10	/* index 0 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR11	/* index 1 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR12	/* index 2 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR13	/* index 3 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR14	/* index 4 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR15	/* index 5 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR16	/* index 6 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR17	/* index 7 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR18	/* index 8 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR19	/* index 9 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR1A	/* index 10 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR1B	/* index 11 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR1C	/* index 12 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR1D	/* index 13 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR1E	/* index 14 */
	bx	lr
	ldcopr16	r0,r1, AMEVCNTR1F	/* index 15 */
	bx	lr
endfunc amu_group1_cnt_read_internal
