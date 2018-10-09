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
	str	wzr, [x0]
	ret
endfunc init_spinlock

func spin_lock
	mov	w2, #1
	sevl
l1:	wfe
l2:	ldaxr	w1, [x0]
	cbnz	w1, l1
	stxr	w1, w2, [x0]
	cbnz	w1, l2
	ret
endfunc spin_lock


func spin_unlock
	stlr	wzr, [x0]
	ret
endfunc spin_unlock
