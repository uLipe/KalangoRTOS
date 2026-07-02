/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Board console public API — boards/qemu_tc3xx/board_console.h
 *
 * Exposes character output via an IPC-backed service thread.
 * Callers use this API directly; no IPC primitives are exposed.
 *
 * Usage:
 *   board_console_start(info);  -- call once from board_services_init()
 *   board_console_putc('x');
 *   board_console_puts("hello\n");
 */

#ifndef BOARD_CONSOLE_H
#define BOARD_CONSOLE_H

#include <ul/microkernel.h>

/*
 * Spawn the console service thread and initialise the internal IPC endpoint.
 * Must be called from the root thread before any board_console_putc/puts call.
 * Returns the TID of the spawned server thread.
 */
ul_tid_t board_console_start(const ul_boot_info_t *info);

/* Write one character to the console (blocking IPC call). */
void board_console_putc(char c);

/* Write a NUL-terminated string to the console, one character at a time. */
void board_console_puts(const char *s);

#endif /* BOARD_CONSOLE_H */
