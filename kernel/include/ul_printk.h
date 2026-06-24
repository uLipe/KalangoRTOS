/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Kernel-internal debug log — kernel/include/ul_printk.h
 *
 * NOT part of the public userspace API.  Only kernel source and board
 * platform files may include this header.
 *
 * ul_printk(fmt, ...) supports: %c %s %d %i %u %x %X %p %lu %lx %zu %%
 * No float, no heap allocation.
 *
 * When UL_CONFIG_DEBUG_PRINTK == 0 every call compiles to ((void)0).
 *
 * Output is routed through ul_printk_char_out(char c), a weak symbol
 * that boards override (e.g. boards/qemu_tc27x/qemu_console.c).
 */

#ifndef UL_PRINTK_H
#define UL_PRINTK_H

#include <ul/config.h>

void ul_printk_char_out(char c);

#if UL_CONFIG_DEBUG_PRINTK

__attribute__((format(printf, 1, 2)))
void _ul_printk(const char *fmt, ...);

#define ul_printk(fmt, ...)  _ul_printk(fmt, ##__VA_ARGS__)
#define UL_LOG_DBG(fmt, ...) _ul_printk("[DBG] " fmt "\n", ##__VA_ARGS__)
#define UL_LOG_ERR(fmt, ...) _ul_printk("[ERR] " fmt "\n", ##__VA_ARGS__)

#else

#define ul_printk(fmt, ...)  ((void)0)
#define UL_LOG_DBG(fmt, ...) ((void)0)
#define UL_LOG_ERR(fmt, ...) ((void)0)

#endif /* UL_CONFIG_DEBUG_PRINTK */

#endif /* UL_PRINTK_H */
