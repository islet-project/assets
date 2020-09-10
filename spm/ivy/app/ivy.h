/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IVY_H
#define IVY_H

#include <stdint.h>

/* Linker symbols used to figure out the memory layout of the S-EL1 shim. */
extern uintptr_t __SHIM_TEXT_START__, __SHIM_TEXT_END__;
#define SHIM_TEXT_START		((uintptr_t)&__SHIM_TEXT_START__)
#define SHIM_TEXT_END		((uintptr_t)&__SHIM_TEXT_END__)

extern uintptr_t __SHIM_RODATA_START__, __SHIM_RODATA_END__;
#define SHIM_RODATA_START	((uintptr_t)&__SHIM_RODATA_START__)
#define SHIM_RODATA_END		((uintptr_t)&__SHIM_RODATA_END__)

extern uintptr_t __SHIM_DATA_START__, __SHIM_DATA_END__;
#define SHIM_DATA_START		((uintptr_t)&__SHIM_DATA_START__)
#define SHIM_DATA_END		((uintptr_t)&__SHIM_DATA_END__)

extern uintptr_t __SHIM_BSS_START__, __SHIM_BSS_END__;
#define SHIM_BSS_START		((uintptr_t)&__SHIM_BSS_START__)
#define SHIM_BSS_END		((uintptr_t)&__SHIM_BSS_END__)

/* Linker symbols used to figure out the memory layout of Ivy (S-EL0). */
extern uintptr_t __TEXT_START__, __TEXT_END__;
#define IVY_TEXT_START		((uintptr_t)&__TEXT_START__)
#define IVY_TEXT_END		((uintptr_t)&__TEXT_END__)

extern uintptr_t __RODATA_START__, __RODATA_END__;
#define IVY_RODATA_START	((uintptr_t)&__RODATA_START__)
#define IVY_RODATA_END		((uintptr_t)&__RODATA_END__)

extern uintptr_t __DATA_START__, __DATA_END__;
#define IVY_DATA_START		((uintptr_t)&__DATA_START__)
#define IVY_DATA_END		((uintptr_t)&__DATA_END__)

extern uintptr_t __BSS_START__, __BSS_END__;
#define IVY_BSS_START		((uintptr_t)&__BSS_START__)
#define IVY_BSS_END		((uintptr_t)&__BSS_END__)

#endif /* __IVY_H__ */
