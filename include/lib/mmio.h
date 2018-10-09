/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MMIO_H__
#define __MMIO_H__

#include <stdint.h>

static inline void mmio_write_8(uintptr_t addr, uint8_t value)
{
	*(volatile uint8_t*)addr = value;
}

static inline uint8_t mmio_read_8(uintptr_t addr)
{
	return *(volatile uint8_t*)addr;
}

static inline void mmio_write_32(uintptr_t addr, uint32_t value)
{
	*(volatile uint32_t*)addr = value;
}

static inline uint32_t mmio_read_32(uintptr_t addr)
{
	return *(volatile uint32_t*)addr;
}

static inline void mmio_write_64(uintptr_t addr, uint64_t value)
{
	*(volatile uint64_t*)addr = value;
}

static inline uint64_t mmio_read_64(uintptr_t addr)
{
	return *(volatile uint64_t*)addr;
}

#endif /* __MMIO_H__ */
