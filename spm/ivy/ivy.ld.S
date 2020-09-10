/*
 * Copyright (c) 2018-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ivy_def.h>
#include <platform_def.h>
#include <xlat_tables_defs.h>

OUTPUT_FORMAT(PLATFORM_LINKER_FORMAT)
OUTPUT_ARCH(PLATFORM_LINKER_ARCH)
ENTRY(shim_entrypoint)

SECTIONS
{
    . = IVY_IMAGE_BASE;

    ASSERT(. == ALIGN(PAGE_SIZE),
           "TEXT_START address is not aligned to PAGE_SIZE.")

    /*----------------- START S-EL1 SHIM ----------------*/

    .shim_text : {
        __SHIM_TEXT_START__ = .;
        *spm_shim_entrypoint.o(.text*)
        *(.vectors)
        . = NEXT(PAGE_SIZE);
        __SHIM_TEXT_END__ = .;
    }

    .shim_rodata : {
        . = ALIGN(PAGE_SIZE);
        __SHIM_RODATA_START__ = .;

        . = NEXT(PAGE_SIZE);
        __SHIM_RODATA_END__ = .;
    }

    .shim_data : {
        . = ALIGN(PAGE_SIZE);
        __SHIM_DATA_START__ = .;

        . = NEXT(PAGE_SIZE);
        __SHIM_DATA_END__ = .;
    }

    .shim_bss (NOLOAD) : {
        . = ALIGN(PAGE_SIZE);
        __SHIM_BSS_START__ = .;

        *(.bss.shim_stacks)
        *(.bss.tf_base_xlat_table)
        *(.bss.tf_mmap)
        *xlat_tables_context.o(COMMON)
	*xlat_tables_context.o(xlat_table)

        . = NEXT(PAGE_SIZE);
        __SHIM_BSS_END__ = .;
    }

    /*----------------- END S-EL1 SHIM ----------------*/

    .text : {
        __TEXT_START__ = .;
        *ivy_entrypoint.o(.text*)
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
         * prior to enabling the MMU, so having it in RO is better for
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
        . = NEXT(PAGE_SIZE);
        __DATA_END__ = .;
    }

    /*
     * .rela.dyn needs to come after .data for the read-elf utility
     * to parse this section correctly. Ensure 8-byte alignment so
     * that the fields of RELA data structure are aligned.
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
        . = NEXT(PAGE_SIZE);
        __BSS_END__ = .;
    }
}
