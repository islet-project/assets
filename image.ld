IMAGE_BASE = 0x80000000;
OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(gateway_entry)
MEMORY {
    RAM (rwx): ORIGIN = IMAGE_BASE, LENGTH = 0x2000000
}
SECTIONS
{
    . = IMAGE_BASE;
    ASSERT(. == ALIGN(0x1000),
           "TEXT_START address is not aligned to PAGE_SIZE.")
    .text : {
        __TEXT_START__ = .;
        KEEP(*(.head.text))
        *(.text*)
        . = ALIGN(0x1000);
        __TEXT_END__ = .;
    }
    .rodata : {
        . = ALIGN(0x1000);
        __RODATA_START__ = .;
        *(.rodata*)
        . = ALIGN(0x1000);
        __RODATA_END__ = .;
    }
    .data : {
        . = ALIGN(0x1000);
        __DATA_START__ = .;
        *(.data*)
        . = ALIGN(0x1000);
        __BSS_START__ = .;
        *(SORT_BY_ALIGNMENT(.bss*))
        *(COMMON)
        . = ALIGN(0x1000);
        __BSS_END__ = .;
        __DATA_END__ = .;
    } >RAM
}
PHDRS
{
  .text PT_LOAD FLAGS(7);
  .rodata PT_LOAD FLAGS(7);
  .data PT_LOAD FLAGS(7);
}
