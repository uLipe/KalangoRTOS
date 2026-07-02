/*
 * tests/boot/boot_test.ld
 *
 * Minimal linker script for the boot integration test.
 * Mirrors the real TC27x QEMU memory map used by tools/hello/linker.ld:
 *   PFLASH  0x80000000 — code + rodata + data LMA
 *   SRAM    0x70000000 — stack, CSA pool, data VMA, bss
 *
 * SRAM must be at 0x7xxx_xxxx so that arch/tricore/startup.S CSA formula
 * (link = 0x00070000 | extr.u(addr, 6, 16)) produces valid link words.
 *
 * Not a production linker script — used only to exercise the boot path.
 */

OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_start)

MEMORY {
    PFLASH  (rx)  : ORIGIN = 0x80000000, LENGTH = 4M
    SRAM    (rwx) : ORIGIN = 0x70000000, LENGTH = 240K
}

_ulmk_kernel_stack_size = 4096;
_ulmk_isr_stack_size    = 2048;
_ulmk_csa_pool_frames   = 256;

SECTIONS {
    .startup ORIGIN(PFLASH) : {
        KEEP(*(.text._start))
    } > PFLASH

    .vectors : ALIGN(256) {
        KEEP(*(.trap_table))
        . = ALIGN(256);
        KEEP(*(.int_table))
    } > PFLASH

    .text : {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > PFLASH

    __ulmk_kernel_data_load = LOADADDR(.kernel_data);
    .kernel_data : {
        _ulmk_kernel_data_start = .;
        *(.data .data.*)
        *(.sdata .sdata.*)
        /*
         * TriCore GCC -fdata-sections creates one section per global variable
         * with the variable name as the section name (e.g. .sched_current,
         * .tcb_pool).  These WA PROGBITS sections are not matched by .data*
         * above.  The catch-all below collects them; by this point .text* and
         * .rodata* have already been consumed by earlier rules.
         */
        *(*)
        . = ALIGN(4);
        _ulmk_kernel_data_end = .;
    } > SRAM AT > PFLASH

    .bss (NOLOAD) : {
        _ulmk_kernel_bss_start = .;
        *(.bss .bss.*)
        *(.kernel.bss)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4);
        _ulmk_kernel_bss_end = .;
    } > SRAM
    PROVIDE(__bss_start = _ulmk_kernel_bss_start);
    PROVIDE(__bss_end   = _ulmk_kernel_bss_end);

    .csa_pool (NOLOAD) : ALIGN(64) {
        _ulmk_csa_pool_start = .;
        . += _ulmk_csa_pool_frames * 64;
        _ulmk_csa_pool_end = .;
    } > SRAM

    .stack (NOLOAD) : ALIGN(8) {
        . += _ulmk_kernel_stack_size;
        _ulmk_kernel_stack_top = .;
    } > SRAM

    .isr_stack (NOLOAD) : ALIGN(8) {
        _ulmk_isr_stack_bottom = .;
        . += _ulmk_isr_stack_size;
        _ulmk_isr_stack_top = .;
    } > SRAM

    /* Small-data anchors — point to base of SRAM; no real small-data in test. */
    _small_data_  = ORIGIN(SRAM);
    _small_data2_ = ORIGIN(SRAM);
    _small_data3_ = ORIGIN(SRAM);
    _small_data4_ = ORIGIN(SRAM);

    PROVIDE(end      = __bss_end);
    PROVIDE(__HEAP     = __bss_end);
    PROVIDE(__HEAP_END = ORIGIN(SRAM) + LENGTH(SRAM) - _ulmk_kernel_stack_size);

    /*
     * User pool: RAM from the top of the ISR stack (end of all static
     * sections) to the end of SRAM.  Must NOT start at __bss_end because
     * .csa_pool, .stack and .isr_stack are placed after .bss.
     */
    _ulmk_user_pool_start = _ulmk_isr_stack_top;
    _ulmk_user_pool_end   = ORIGIN(SRAM) + LENGTH(SRAM);
}
