/*
 * tests/boot/boot_test_arm.ld — ARM Cortex-M integration-test linker script.
 * Mirrors boot_test_riscv.ld but places the Cortex-M vector table at the flash
 * origin (VTOR reset value) and keeps startup/trap/ctx sections resident.
 *
 * The region origins are board inputs supplied via --defsym so one script
 * serves both Cortex-M boards (arm.mk sets them per ARM_BOARD):
 *   mps2-an500 (v7-M): flash 0x00000000, sram 0x20000000, periph 0x40000000
 *   mps2-an505 (v8-M): flash 0x10000000, sram 0x38000000, periph 0x50000000
 *     — the AN505 boots Secure and resets from the Secure code alias, so all
 *       regions use the Secure (IDAU bit 28 set) aliases.
 */

OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
	KERNEL_FLASH  (rx)  : ORIGIN = ULMK_FLASH_BASE,  LENGTH = 512K
	SRAM          (rwx) : ORIGIN = ULMK_SRAM_BASE,   LENGTH = 1024K
	PERIPH        (rw)  : ORIGIN = ULMK_PERIPH_BASE, LENGTH = 256M
}

_ulmk_kernel_stack_size = 4096;
_ulmk_isr_stack_size    = 2048;

SECTIONS {
	.isr_vector ORIGIN(KERNEL_FLASH) : ALIGN(512) {
		_ulmk_kernel_exec_start = .;
		KEEP(*(.isr_vector))
	} > KERNEL_FLASH

	.startup : ALIGN(4) {
		KEEP(*(.text._start))
	} > KERNEL_FLASH

	.trap : {
		KEEP(*(.text.trap))
		KEEP(*(.text.ctx_switch))
	} > KERNEL_FLASH

	.kernel_text : {
		*libtest_kernel.a:(.text .text.* .rodata .rodata.*)
		. = ALIGN(32);
		_ulmk_kernel_text_end = .;
		_ulmk_kernel_exec_end = .;
	} > KERNEL_FLASH

	_ulmk_user_text_start = .;

	.user_text : {
		*(.text .text.* .rodata .rodata.*)
		. = ALIGN(32);
		_ulmk_user_text_end = .;
	} > KERNEL_FLASH

	__ulmk_kernel_data_load = LOADADDR(.kernel_data);
	.kernel_data : {
		_ulmk_kernel_data_start = .;
		*libtest_kernel.a:(.data .data.*)
		*libtest_kernel.a:(*)
		. = ALIGN(4);
		_ulmk_kernel_data_end = .;
	} > SRAM AT > KERNEL_FLASH

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
		*(.data .data.*)
		. = ALIGN(4);
		_ulmk_user_data_end = .;
	} > SRAM AT > KERNEL_FLASH

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

	PROVIDE(end        = __bss_end);
	PROVIDE(__HEAP     = _ulmk_user_pool_start);
	PROVIDE(__HEAP_END = _ulmk_user_pool_end);
}

PROVIDE(_ulmk_mem_flash_base  = ORIGIN(KERNEL_FLASH));
PROVIDE(_ulmk_mem_flash_end   = ORIGIN(KERNEL_FLASH) + LENGTH(KERNEL_FLASH));
PROVIDE(_ulmk_mem_ram_base    = ORIGIN(SRAM));
PROVIDE(_ulmk_mem_ram_end     = ORIGIN(SRAM) + LENGTH(SRAM));
PROVIDE(_ulmk_mem_shared_base = ORIGIN(SRAM));
PROVIDE(_ulmk_mem_shared_end  = ORIGIN(SRAM));
PROVIDE(_ulmk_mem_periph_base = ORIGIN(PERIPH));
PROVIDE(_ulmk_mem_periph_end  = ORIGIN(PERIPH) + LENGTH(PERIPH));
