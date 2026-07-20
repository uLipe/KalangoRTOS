/* SPDX-License-Identifier: MIT */
/* boards/qemu_riscv_virt/board_console.h */

#ifndef BOARD_CONSOLE_H
#define BOARD_CONSOLE_H

#include <ulmk/microkernel.h>

#define BOARD_CONSOLE_UART_BASE		0x10000000u
#define BOARD_CONSOLE_UART_MAP_SIZE	0x100u

ulmk_tid_t board_console_start(const ulmk_boot_info_t *info);
void       board_console_putc(char c);
void       board_console_puts(const char *s);

#endif /* BOARD_CONSOLE_H */
