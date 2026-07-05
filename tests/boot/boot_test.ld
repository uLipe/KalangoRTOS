/*
 * tests/boot/boot_test.ld
 *
 * Integration-test linker script — mirrors production MPU section layout.
 * Kernel sections are collected from libtest_kernel.a (see integ_kernel.mk).
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
        _ulmk_kernel_exec_start = .;
    } > PFLASH

    .vectors : ALIGN(256) {
        KEEP(*(.trap_table))
        . = ALIGN(256);
        KEEP(*(.int_table))
    } > PFLASH

    .kernel_text : {
        *libtest_kernel.a:(.text .text.* .rodata .rodata.*)
        . = ALIGN(32);
        _ulmk_kernel_text_end = .;
        _ulmk_kernel_exec_end = .;
    } > PFLASH

    _ulmk_user_text_start = .;

    .user_text : {
        *(.text .text.* .rodata .rodata.*)
        . = ALIGN(32);
        _ulmk_user_text_end = .;
    } > PFLASH

    __ulmk_kernel_data_load = LOADADDR(.kernel_data);
    .kernel_data : {
        _ulmk_kernel_data_start = .;
        *libtest_kernel.a:(.data .data.* .sdata .sdata.*)
        *libtest_kernel.a:(*)
        . = ALIGN(4);
        _ulmk_kernel_data_end = .;
    } > SRAM AT > PFLASH

    .kernel_bss (NOLOAD) : {
        _ulmk_kernel_bss_start = .;
        *libtest_kernel.a:(.bss .bss.* .sbss .sbss.*)
        *libtest_kernel.a:(COMMON)
        . = ALIGN(4);
        _ulmk_kernel_bss_end = .;
    } > SRAM

    .csa_pool (NOLOAD) : ALIGN(64) {
        _ulmk_csa_pool_start = .;
        . += _ulmk_csa_pool_frames * 64;
        _ulmk_csa_pool_end = .;
    } > SRAM

    .stack (NOLOAD) : ALIGN(8) {
        . += _ulmk_kernel_stack_size;
        _ulmk_kernel_stack_top = .;
    } > SRAM

    _ulmk_kernel_ram_end = .;

    _small_data_  = ORIGIN(SRAM);
    _small_data2_ = ORIGIN(SRAM);
    _small_data3_ = ORIGIN(SRAM);
    _small_data4_ = ORIGIN(SRAM);

    _ulmk_user_ram_start = .;

    .isr_stack (NOLOAD) : ALIGN(8) {
        _ulmk_isr_stack_bottom = .;
        . += _ulmk_isr_stack_size;
        _ulmk_isr_stack_top = .;
    } > SRAM

    __ulmk_user_data_load = LOADADDR(.user_data);
    .user_data : {
        _ulmk_user_data_start = .;
        *(.data .data.* .sdata .sdata.*)
        . = ALIGN(4);
        _ulmk_user_data_end = .;
    } > SRAM AT > PFLASH

    .user_bss (NOLOAD) : {
        *libtest_kernel.a:(.user_bss .user_bss.*)
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4);
    } > SRAM

    PROVIDE(__bss_start = _ulmk_kernel_bss_start);
    PROVIDE(__bss_end   = _ulmk_kernel_bss_end);

    _ulmk_user_pool_start = .;
    . = ORIGIN(SRAM) + LENGTH(SRAM);
    _ulmk_user_pool_end = .;

    PROVIDE(end      = __bss_end);
    PROVIDE(__HEAP     = _ulmk_user_pool_start);
    PROVIDE(__HEAP_END = _ulmk_user_pool_end);
}
