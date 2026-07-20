/* SPDX-License-Identifier: MIT */
/* Kernel-side early console — boards/qemu_riscv_virt/qemu_printk_hook.c */

#include <stdint.h>
#include <kernel/include/ulmk_printk.h>

#define UART_BASE		0x10000000u
#define UART_LSR		5u
#define UART_LSR_TX_IDLE	(1u << 5)

static void uart_putc(char c)
{
	volatile uint8_t *uart = (volatile uint8_t *)(uintptr_t)UART_BASE;

	while (!(uart[UART_LSR] & UART_LSR_TX_IDLE))
		;
	uart[0] = (uint8_t)c;
}

void ulmk_printk_char_out(char c)
{
	uart_putc(c);
}
