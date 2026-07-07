/* SPDX-License-Identifier: MIT */
/*
 * Kernel-side QEMU virt console hooks — boards/qemu_tc3xx/qemu_printk_hook.c
 *
 * Linked into libulmk_kernel.a; not userspace board code.
 */

#include <stdint.h>
#include <kernel/include/ulmk_printk.h>
#include <board_config.h>

#define VIRT_BASE        ULMK_BOARD_VIRT_CONSOLE_BASE
#define VIRT_PUTCHAR_OFF 0x20U
#define VIRT_EXIT_OFF    0x28U

void ulmk_printk_char_out(char c)
{
	volatile uint32_t *reg =
		(volatile uint32_t *)(VIRT_BASE + VIRT_PUTCHAR_OFF);

	*reg = (uint32_t)(uint8_t)c;
}

__attribute__((noreturn)) void ulmk_sim_exit(int code)
{
	volatile uint32_t *reg =
		(volatile uint32_t *)(VIRT_BASE + VIRT_EXIT_OFF);

	*reg = (uint32_t)code;
	for (;;)
		;
}
