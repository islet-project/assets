/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
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

static inline void mmio_write32_offset(uintptr_t addr, uint32_t byte_off,
					uint32_t data)
{
	mmio_write_32((uintptr_t)((uint8_t *)addr + byte_off), data);
}

static inline uint32_t mmio_read_32(uintptr_t addr)
{
	return *(volatile uint32_t*)addr;
}

static inline uint32_t mmio_read32_offset(uintptr_t addr, uint32_t byte_off)
{
	return mmio_read_32((uintptr_t)((uint8_t *)addr + byte_off));
}

static inline void mmio_write_64(uintptr_t addr, uint64_t value)
{
	*(volatile uint64_t*)addr = value;
}

static inline void mmio_write64_offset(uintptr_t addr, uint32_t byte_off,
					uint64_t data)
{
	mmio_write_64((uintptr_t)((uint8_t *)addr + byte_off), data);
}

static inline uint64_t mmio_read_64(uintptr_t addr)
{
	return *(volatile uint64_t*)addr;
}

#endif /* __MMIO_H__ */
