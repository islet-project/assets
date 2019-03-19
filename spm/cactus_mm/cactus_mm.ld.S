/*
 * Copyright (c) 2017-2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>
#include <xlat_tables_defs.h>

OUTPUT_FORMAT(PLATFORM_LINKER_FORMAT)
OUTPUT_ARCH(PLATFORM_LINKER_ARCH)
ENTRY(cactus_mm_entrypoint)

SECTIONS
{
    ASSERT(. == ALIGN(PAGE_SIZE),
           "TEXT_START address is not aligned to PAGE_SIZE.")

    .text : {
        __TEXT_START__ = .;
        *cactus_mm_entrypoint.o(.text*)
        *(.text*)
        *(.vectors)
        . = NEXT(PAGE_SIZE);
        __TEXT_END__ = .;
    }

    .rodata : {
        . = ALIGN(PAGE_SIZE);
        __RODATA_START__ = .;
        *(.rodata*)
        . = NEXT(PAGE_SIZE);
        __RODATA_END__ = .;
    }


    .data : {
        . = ALIGN(PAGE_SIZE);
        __RWDATA_START__ = .;
        *(.data*)
    }

    .bss : {
        . = ALIGN(16);
        __BSS_START__ = .;
        *(SORT_BY_ALIGNMENT(.bss*))
        *(COMMON)
        . = NEXT(PAGE_SIZE);
        __BSS_END__ = .;
        __RWDATA_END__ = .;
    }
}
