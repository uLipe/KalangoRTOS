/* SPDX-License-Identifier: MIT */
/* Kernel-side early console — boards/qemu_mps2_an500/qemu_printk_hook.c
 *
 * Direct polled CMSDK APB UART0 output for kernel printk (runs privileged, so
 * the MPU background region grants access without a mapping).
 */

#include <stdint.h>
#include <kernel/include/ulmk_printk.h>
#include "board_config.h"

#define UART_DATA	0u	/* [0x00] TX/RX data          */
#define UART_STATE	1u	/* [0x04] bit0 = TX buffer full */
#define UART_CTRL	2u	/* [0x08] bit0 = TX enable    */

#define UART_STATE_TXFULL	(1u << 0)
#define UART_CTRL_TXEN		(1u << 0)

static void uart_putc(char c)
{
	volatile uint32_t *uart =
		(volatile uint32_t *)(uintptr_t)BOARD_CONSOLE_UART_BASE;

	uart[UART_CTRL] = UART_CTRL_TXEN;
	while (uart[UART_STATE] & UART_STATE_TXFULL)
		;
	uart[UART_DATA] = (uint32_t)(uint8_t)c;
}

void ulmk_printk_char_out(char c)
{
	uart_putc(c);
}
