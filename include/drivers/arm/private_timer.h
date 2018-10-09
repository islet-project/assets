/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PRIVATE_TIMER_H__
#define __PRIVATE_TIMER_H__

void private_timer_start(unsigned long timeo);
void private_timer_stop(void);
void private_timer_save(void);
void private_timer_restore(void);

#endif /* __PRIVATE_TIMER_H__ */
