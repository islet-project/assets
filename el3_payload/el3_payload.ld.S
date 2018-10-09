/*
 * Copyright (c) 2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH("aarch64")
ENTRY(entrypoint)

MEMORY {
    RAM (rwx): ORIGIN = DRAM_BASE, LENGTH = (DRAM_BASE + DRAM_SIZE)
}

SECTIONS
{
    . = DRAM_BASE;

    ro . : {
        */entrypoint.o(.text)
        *(.text*)
        *(.rodata*)
    } >RAM

    .data : {
        *(.data*)
    } >RAM
}
