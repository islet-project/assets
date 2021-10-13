/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>
#include <xlat_tables_defs.h>

OUTPUT_FORMAT(PLATFORM_LINKER_FORMAT)
OUTPUT_ARCH(PLATFORM_LINKER_ARCH)
ENTRY(ns_bl2u_entrypoint)

MEMORY {
    RAM (rwx): ORIGIN = NS_BL2U_BASE, LENGTH = NS_BL2U_LIMIT - NS_BL2U_BASE
}

SECTIONS
{
    . = NS_BL2U_BASE;
    ASSERT(. == ALIGN(PAGE_SIZE),
           "NS_BL2U_BASE address is not aligned on a page boundary.")

    .text . : {
        __TEXT_START__ = .;
        *ns_bl2u_entrypoint.o(.text*)
        *(.text*)
        *(.vectors)
        . = ALIGN(PAGE_SIZE);
        __TEXT_END__ = .;
    } >RAM

    .rodata . : {
        __RODATA_START__ = .;
        *(.rodata*)
        /*
         * Memory page(s) mapped to this section will be marked as
         * read-only, non-executable.  No RW data from the next section must
         * creep in.  Ensure the rest of the current memory page is unused.
         */
        . = ALIGN(PAGE_SIZE);
        __RODATA_END__ = .;
    } >RAM

    __RW_START__ = .;

    .data . : {
        __DATA_START__ = .;
        *(.data*)
        __DATA_END__ = .;
    } >RAM

    stacks (NOLOAD) : {
        __STACKS_START__ = .;
        *(ns_bl_normal_stacks)
        __STACKS_END__ = .;
    } >RAM

    /*
     * The .bss section gets initialised to 0 at runtime.
     * Its base address must be 16-byte aligned.
     */
    .bss : ALIGN(16) {
        __BSS_START__ = .;
        *(SORT_BY_ALIGNMENT(.bss*))
        *(COMMON)
        __BSS_END__ = .;
    } >RAM

    /*
     * The xlat_table section is for full, aligned page tables (4K).
     * Removing them from .bss avoids forcing 4K alignment on
     * the .bss section and eliminates the unecessary zero init
     */
    xlat_table (NOLOAD) : {
        *(xlat_table)
    } >RAM

    __NS_BL2U_END__ = .;
    __RW_END__ = .;

    __BSS_SIZE__ = SIZEOF(.bss);

    ASSERT(. <= NS_BL2U_LIMIT, "NS_BL2U image has exceeded its limit.")
}
