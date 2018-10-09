/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __IO_MEMMAP_H__
#define __IO_MEMMAP_H__

struct io_dev_connector;

int register_io_dev_memmap(const struct io_dev_connector **dev_con);

#endif /* __IO_MEMMAP_H__ */
