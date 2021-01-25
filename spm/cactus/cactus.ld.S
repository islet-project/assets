/*
 * Copyright (c) 2017-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cactus_def.h>
#include <platform_def.h>
#include <xlat_tables_defs.h>

OUTPUT_FORMAT(PLATFORM_LINKER_FORMAT)
OUTPUT_ARCH(PLATFORM_LINKER_ARCH)
ENTRY(cactus_entrypoint)

SECTIONS
{
    . = CACTUS_IMAGE_BASE;

    ASSERT(. == ALIGN(PAGE_SIZE),
           "TEXT_START address is not aligned to PAGE_SIZE.")

    .text : {
        __TEXT_START__ = .;
        *cactus_entrypoint.o(.text*)
        *(.text*)
        *(.vectors)
        . = NEXT(PAGE_SIZE);
        __TEXT_END__ = .;
    }

    .rodata : {
        . = ALIGN(PAGE_SIZE);
        __RODATA_START__ = .;
        *(.rodata*)

        /*
         * Keep the .got section in the RO section as it is patched
         * prior to enabling the MMU and having the .got in RO is better for
         * security. GOT is a table of addresses so ensure 8-byte alignment.
         */
        . = ALIGN(8);
        __GOT_START__ = .;
        *(.got)
        __GOT_END__ = .;

        . = NEXT(PAGE_SIZE);
        __RODATA_END__ = .;

    }

    .data : {
        . = ALIGN(PAGE_SIZE);
        __DATA_START__ = .;
        *(.data*)
        . = ALIGN(PAGE_SIZE);
        cactus_cmd_handler_begin = .;
        KEEP(*(.cactus_handler))
        cactus_cmd_handler_end = .;
        . = NEXT(PAGE_SIZE);
        __DATA_END__ = .;
    }

    /*
     * .rela.dyn needs to come after .data for the read-elf utility to parse
     * this section correctly. Ensure 8-byte alignment so that the fields of
     * RELA data structure are aligned.
     */
    . = ALIGN(8);
    __RELA_START__ = .;
    .rela.dyn . : {
    }
    __RELA_END__ = .;

    .bss (NOLOAD) : {
        . = ALIGN(PAGE_SIZE);
        __BSS_START__ = .;
        *(SORT_BY_ALIGNMENT(.bss*))
        *(COMMON)
	*(xlat_table*)
        . = NEXT(PAGE_SIZE);
        __BSS_END__ = .;
    }
}
