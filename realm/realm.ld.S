/*
 * Copyright (c) 2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


ENTRY(realm_entrypoint)

#include <realm_def.h>

MEMORY {

    RAM (rwx): ORIGIN = 0x0, LENGTH = REALM_MAX_LOAD_IMG_SIZE
}

SECTIONS
{
     ASSERT(. == ALIGN(PAGE_SIZE),
           "TEXT_START address is not aligned to PAGE_SIZE.")
    .text : {
        __REALM_TEXT_START__ = .;
        *realm_entrypoint.o(.text*)
        *(.text*)
        *(.vectors)
        . = NEXT(PAGE_SIZE);
        __REALM_TEXT_END__ = .;
    }> RAM

    .rodata : {
        . = ALIGN(PAGE_SIZE);
        __REALM_RODATA_START__ = .;
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
        __REALM_RODATA_END__ = .;

    }> RAM

    .data : {
        . = ALIGN(PAGE_SIZE);
        __REALM_DATA_START__ = .;
        *(.data*)
        . = ALIGN(PAGE_SIZE);
        . = NEXT(PAGE_SIZE);
        __REALM_DATA_END__ = .;
    }> RAM

    /*
     * .rela.dyn needs to come after .data for the read-elf utility to parse
     * this section correctly. Ensure 8-byte alignment so that the fields of
     * RELA data structure are aligned.
     */
    . = ALIGN(8);
    __RELA_START__ = .;
    .rela.dyn . : {
    }> RAM
    __RELA_END__ = .;

    .bss (NOLOAD) : {
        . = ALIGN(PAGE_SIZE);
        __REALM_BSS_START__ = .;
        *(SORT_BY_ALIGNMENT(.bss*))
        *(COMMON)
        . = NEXT(PAGE_SIZE);
        __REALM_BSS_END__ = .;
    }> RAM
    __REALM_BSS_SIZE__ = SIZEOF(.bss);
}
