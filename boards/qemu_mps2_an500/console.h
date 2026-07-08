/* SPDX-License-Identifier: MIT */
/* boards/qemu_mps2_an500/console.h — userspace console API */

#ifndef BOARD_QEMU_CONSOLE_H
#define BOARD_QEMU_CONSOLE_H

void console_init(void);
void console_puts(const char *s);
void console_printf(const char *fmt, ...);

#endif /* BOARD_QEMU_CONSOLE_H */
