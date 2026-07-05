/* SPDX-License-Identifier: MIT */
/*
 * Kernel-side QEMU hooks for integration tests (linked in libtest_kernel.a).
 */

#include <stdint.h>
#include <kernel/include/ulmk_printk.h>

#define VIRT_BASE        0xBF000000UL
#define VIRT_PUTCHAR_OFF 0x20U

void ulmk_printk_char_out(char c)
{
	volatile uint32_t *reg =
		(volatile uint32_t *)(VIRT_BASE + VIRT_PUTCHAR_OFF);

	*reg = (uint32_t)(uint8_t)c;
}
