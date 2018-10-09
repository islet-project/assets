/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>
#include <xlat_tables_defs.h>

OUTPUT_FORMAT(PLATFORM_LINKER_FORMAT)
OUTPUT_ARCH(PLATFORM_LINKER_ARCH)
ENTRY(ns_bl1u_entrypoint)

MEMORY {
    ROM (rx): ORIGIN = NS_BL1U_RO_BASE, LENGTH = NS_BL1U_RO_LIMIT - NS_BL1U_RO_BASE
    RAM (rwx): ORIGIN = NS_BL1U_RW_BASE, LENGTH = NS_BL1U_RW_LIMIT - NS_BL1U_RW_BASE
}

SECTIONS
{
    . = NS_BL1U_RO_BASE;
    ASSERT(. == ALIGN(PAGE_SIZE),
           "NS_BL1U_RO_BASE address is not aligned on a page boundary.")

    ro . : {
        __RO_START__ = .;
        *ns_bl1u_entrypoint.o(.text*)
        *(.text*)
        *(.rodata*)
        __RO_END__ = .;
    } >ROM

    /*
     * The .data section gets copied from ROM to RAM at runtime.
     * Its LMA must be 16-byte aligned.
     * Its VMA must be page-aligned as it marks the first read/write page.
     */
    . = NS_BL1U_RW_BASE;
    ASSERT(. == ALIGN(PAGE_SIZE),
           "NS_BL1U_RW_BASE address is not aligned on a page boundary.")
    .data . : ALIGN(16) {
        __DATA_RAM_START__ = .;
        *(.data*)
        __DATA_RAM_END__ = .;
    } >RAM AT>ROM

    stacks . (NOLOAD) : {
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

    __NS_BL1U_RAM_START__ = ADDR(.data);
    __NS_BL1U_RAM_END__ = .;

    __DATA_ROM_START__ = LOADADDR(.data);
    __DATA_SIZE__ = SIZEOF(.data);

    /*
     * The .data section is the last PROGBITS section so its end marks the end
     * of the read-only part of NS_BL1U's binary.
     */
    ASSERT(__DATA_ROM_START__ + __DATA_SIZE__ <= NS_BL1U_RO_LIMIT,
           "NS_BL1U's RO section has exceeded its limit.")

    __BSS_SIZE__ = SIZEOF(.bss);

    ASSERT(. <= NS_BL1U_RW_LIMIT, "NS_BL1U's RW section has exceeded its limit.")
}
