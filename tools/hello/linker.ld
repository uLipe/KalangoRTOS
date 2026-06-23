/*
 * Linker script for TriCore TC27x on QEMU (linumiz KIT_AURIX_TC277_TRB).
 *
 * Memory map (TC27x):
 *   PFLASH  0x80000000  4 MB  — program flash
 *   SRAM    0x70000000  256 KB — CPU0 local SRAM (data + stack + CSA pool)
 */

OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_start)

MEMORY {
    PFLASH (rx)  : ORIGIN = 0x80000000, LENGTH = 4M
    SRAM   (rwx) : ORIGIN = 0x70000000, LENGTH = 256K
}

__stack_size = 4096;

SECTIONS {
    /* _start must land at the very beginning of flash. */
    .startup ORIGIN(PFLASH) : {
        KEEP(*(.startup.text))
    } > PFLASH

    .text : {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > PFLASH

    /* Initialised data: stored in flash (LMA), copied to SRAM (VMA). */
    __data_load = LOADADDR(.data);
    .data : {
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > SRAM AT > PFLASH

    /* CSA pool — must be 64-byte aligned per TriCore CSA addressing. */
    .csa_pool (NOLOAD) : ALIGN(64) {
        *(.csa_pool)
        . = ALIGN(64);
    } > SRAM

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > SRAM

    .stack (NOLOAD) : ALIGN(8) {
        . += __stack_size;
        __stack_top = .;
    } > SRAM

    PROVIDE(end = __bss_end);
    PROVIDE(__HEAP     = __bss_end);
    PROVIDE(__HEAP_END = ORIGIN(SRAM) + LENGTH(SRAM) - __stack_size);
}
