/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Kernel-internal debug log — kernel/include/ulmk_printk.h
 *
 * NOT part of the public userspace API.  Only kernel source and board
 * platform files may include this header.
 *
 * ulmk_printk(fmt, ...) supports: %c %s %d %i %u %x %X %p %lu %lx %zu %%
 * No float, no heap allocation.
 *
 * When ULMK_CONFIG_DEBUG_PRINTK == 0 every call compiles to ((void)0).
 *
 * Output is routed through ulmk_printk_char_out(char c), a weak symbol
 * that boards override (e.g. boards/qemu_tc3xx/qemu_console.c).
 */

/*
 * Enforce kernel-only use.  Files that legitimately need this header must
 * compile with -DULMK_KERNEL_BUILD (added by the kernel CMakeLists / Makefiles).
 * Board console drivers that provide ulmk_printk_char_out() are the only
 * non-kernel files permitted to include this header — they are compiled as
 * part of the kernel image and must also define ULMK_KERNEL_BUILD.
 */
#ifndef ULMK_KERNEL_BUILD
#error "ulmk_printk.h is kernel-internal and must not be included from userspace"
#endif

#ifndef UL_PRINTK_H
#define UL_PRINTK_H

#include <ulmk/config.h>

void ulmk_printk_char_out(char c);

#if ULMK_CONFIG_DEBUG_PRINTK

__attribute__((format(printf, 1, 2)))
void _ulmk_printk(const char *fmt, ...);

#define ulmk_printk(fmt, ...)  _ulmk_printk(fmt, ##__VA_ARGS__)
#define UL_LOG_DBG(fmt, ...) _ulmk_printk("[DBG] " fmt "\n", ##__VA_ARGS__)
#define UL_LOG_ERR(fmt, ...) _ulmk_printk("[ERR] " fmt "\n", ##__VA_ARGS__)

#else

#define ulmk_printk(fmt, ...)  ((void)0)
#define UL_LOG_DBG(fmt, ...) ((void)0)
#define UL_LOG_ERR(fmt, ...) ((void)0)

#endif /* ULMK_CONFIG_DEBUG_PRINTK */

#endif /* UL_PRINTK_H */
