/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SYNC_H__
#define  __SYNC_H__

typedef bool (*exception_handler_t)(void);
void register_custom_sync_exception_handler(exception_handler_t handler);
void unregister_custom_sync_exception_handler(void);

#endif /* __SYNC_H__ */
