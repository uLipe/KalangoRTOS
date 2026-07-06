/*
 * tests/boot/boot_test_riscv.ld — RV32 integration-test linker script
 */

OUTPUT_FORMAT("elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY {
	PFLASH  (rx)  : ORIGIN = 0x80000000, LENGTH = 256K
	SRAM    (rwx) : ORIGIN = 0x80040000, LENGTH = 768K
	PERIPH  (rw)  : ORIGIN = 0x00000000, LENGTH = 512M
}

_ulmk_kernel_stack_size = 4096;
_ulmk_isr_stack_size    = 2048;

SECTIONS {
	.startup ORIGIN(PFLASH) : {
		KEEP(*(.text._start))
		_ulmk_kernel_exec_start = .;
	} > PFLASH

	.trap : {
		KEEP(*(.text.trap))
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
		*libtest_kernel.a:(.data .data.*)
		*libtest_kernel.a:(*)
		. = ALIGN(4);
		_ulmk_kernel_data_end = .;
	} > SRAM AT > PFLASH

	.kernel_bss (NOLOAD) : {
		_ulmk_kernel_bss_start = .;
		*libtest_kernel.a:(.bss .bss.*)
		*libtest_kernel.a:(COMMON)
		. = ALIGN(4);
		_ulmk_kernel_bss_end = .;
	} > SRAM

	.stack (NOLOAD) : ALIGN(8) {
		. += _ulmk_kernel_stack_size;
		_ulmk_kernel_stack_top = .;
	} > SRAM

	_ulmk_kernel_ram_end = .;

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
		*(.user_bss .user_bss.*)
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

PROVIDE(_ulmk_mem_flash_base  = ORIGIN(PFLASH));
PROVIDE(_ulmk_mem_flash_end   = ORIGIN(PFLASH) + LENGTH(PFLASH));
PROVIDE(_ulmk_mem_ram_base    = ORIGIN(SRAM));
PROVIDE(_ulmk_mem_ram_end     = ORIGIN(SRAM) + LENGTH(SRAM));
PROVIDE(_ulmk_mem_shared_base = ORIGIN(SRAM));
PROVIDE(_ulmk_mem_shared_end  = ORIGIN(SRAM));
PROVIDE(_ulmk_mem_periph_base = ORIGIN(PERIPH));
PROVIDE(_ulmk_mem_periph_end  = ORIGIN(PERIPH) + LENGTH(PERIPH));
