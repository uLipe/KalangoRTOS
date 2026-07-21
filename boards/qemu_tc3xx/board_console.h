/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Board console — boards/qemu_tc3xx/board_console.h
 *
 * IPC-backed console: putc / puts / printf.  Format on client; WRITE is
 * one ep_call so multi-core output stays atomic.
 */

#ifndef BOARD_CONSOLE_H
#define BOARD_CONSOLE_H

#include <ulmk/microkernel.h>

ulmk_tid_t board_console_start(const ulmk_boot_info_t *info);
void board_console_putc(char c);
void board_console_puts(const char *s);
void board_console_printf(const char *fmt, ...);

#endif /* BOARD_CONSOLE_H */
