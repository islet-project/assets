/*
 * Copyright (c) 2018-2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QUARK_H
#define QUARK_H

#include <stdint.h>

/* Linker symbols used to figure out the memory layout of Quark. */
extern uintptr_t __TEXT_START__, __TEXT_END__;
#define QUARK_TEXT_START	((uintptr_t)&__TEXT_START__)
#define QUARK_TEXT_END		((uintptr_t)&__TEXT_END__)

extern uintptr_t __RODATA_START__, __RODATA_END__;
#define QUARK_RODATA_START	((uintptr_t)&__RODATA_START__)
#define QUARK_RODATA_END	((uintptr_t)&__RODATA_END__)

extern uintptr_t __DATA_START__, __DATA_END__;
#define QUARK_DATA_START	((uintptr_t)&__DATA_START__)
#define QUARK_DATA_END		((uintptr_t)&__DATA_END__)

extern uintptr_t __BSS_START__, __BSS_END__;
#define QUARK_BSS_START	((uintptr_t)&__BSS_START__)
#define QUARK_BSS_END		((uintptr_t)&__BSS_END__)

#endif /* QUARK_H */
