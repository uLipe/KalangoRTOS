/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Userspace console API — boards/qemu_tc3xx/console.h
 *
 * Provides character output for userspace threads running on the QEMU
 * TC3xx target.  Internally maps the virtual debug device via ul_mem_map
 * (UL_MMAP_PERIPH) so no direct MMIO access is performed before mapping.
 *
 * Usage:
 *   console_init();          -- call once from the root thread
 *   console_puts("hello\n");
 *   console_printf("tick #%u\n", n);
 */

#ifndef QEMU_CONSOLE_H
#define QEMU_CONSOLE_H

#include <stdarg.h>

/*
 * Map the virtual debug device into the calling thread's address space.
 * Must be called once before any console_puts / console_printf call.
 */
void console_init(void);

/* Write a NUL-terminated string to the console. */
void console_puts(const char *s);

/* printf-like output; uses a fixed 256-byte stack buffer. */
__attribute__((format(printf, 1, 2)))
void console_printf(const char *fmt, ...);

#endif /* QEMU_CONSOLE_H */
