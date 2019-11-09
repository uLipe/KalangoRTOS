/*
 * Linker script for TriCore TC27x on QEMU (linumiz KIT_AURIX_TC277_TRB).
 *
 * Memory map (TC27x):
 *   PFLASH  0x80000000  4 MB  — program flash (cached)
 *   SRAM    0x70000000  256 KB — CPU0 local SRAM (data + CSA pool)
 *
 * Vector tables placed at well-known offsets in flash so BIV/BTV can be
 * set at runtime.  CSA pool alignment is 64 bytes (per architecture spec).
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
    /* -----------------------------------------------------------------------
     * .startup — placed first so _start is at the very beginning of flash.
     * ----------------------------------------------------------------------- */
    .startup ORIGIN(PFLASH) : {
        KEEP(*(.startup.text))
    } > PFLASH

    /* -----------------------------------------------------------------------
     * .inttab — interrupt vector table (8 bytes per entry × 256 priorities)
     *           must be 8-byte aligned; BIV will point here.
     * ----------------------------------------------------------------------- */
    .inttab : ALIGN(8) {
        _tricore_int_tab = .;
        KEEP(*(.inttab))
    } > PFLASH

    /* -----------------------------------------------------------------------
     * .traptab — trap vector table (32 bytes per class × 8 classes)
     *            must be 256-byte aligned so BTV OR-addressing gives correct
     *            results (QEMU: PC = BTV | class*32, not BTV + class*32).
     * ----------------------------------------------------------------------- */
    .traptab : ALIGN(256) {
        _tricore_trap_tab = .;
        KEEP(*(.traptab))
    } > PFLASH

    /* -----------------------------------------------------------------------
     * Standard code and read-only data
     * ----------------------------------------------------------------------- */
    .text : {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > PFLASH

    /* -----------------------------------------------------------------------
     * Initialised data: stored in flash (LMA), copied to SRAM (VMA)
     * ----------------------------------------------------------------------- */
    __data_load = LOADADDR(.data);
    .data : {
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > SRAM AT > PFLASH

    /* -----------------------------------------------------------------------
     * CSA pool — 64-byte aligned (required by TriCore CSA addressing)
     * ----------------------------------------------------------------------- */
    .csa_pool (NOLOAD) : ALIGN(64) {
        *(.csa_pool)
        . = ALIGN(64);
    } > SRAM

    /* -----------------------------------------------------------------------
     * Zero-initialised data
     * ----------------------------------------------------------------------- */
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > SRAM

    /* -----------------------------------------------------------------------
     * Stack (grows downward from top of SRAM)
     * ----------------------------------------------------------------------- */
    .stack (NOLOAD) : ALIGN(8) {
        . += __stack_size;
        __stack_top = .;
    } > SRAM

    PROVIDE(end = __bss_end);
    /* newlib sbrk symbols — point to end of BSS; unused since TLSF owns the heap */
    PROVIDE(__HEAP     = __bss_end);
    PROVIDE(__HEAP_END = ORIGIN(SRAM) + LENGTH(SRAM) - __stack_size);
}
