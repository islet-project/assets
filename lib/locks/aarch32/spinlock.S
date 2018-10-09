/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>

	.globl	init_spinlock
	.globl	spin_lock
	.globl	spin_unlock

func init_spinlock
	mov	r1, #0
	str	r1, [r0]
	bx	lr
endfunc init_spinlock

func spin_lock
	mov	r2, #1
1:
	ldrex	r1, [r0]
	cmp	r1, #0
	wfene
	strexeq	r1, r2, [r0]
	cmpeq	r1, #0
	bne	1b
	dmb
	bx	lr
endfunc spin_lock


func spin_unlock
	mov	r1, #0
	stl	r1, [r0]
	bx	lr
endfunc spin_unlock
