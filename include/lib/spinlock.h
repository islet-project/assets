/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

typedef struct spinlock {
	volatile unsigned int lock;
} spinlock_t;

void init_spinlock(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

#endif /* __SPINLOCK_H__ */
