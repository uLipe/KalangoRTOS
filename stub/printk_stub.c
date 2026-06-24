/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Weak no-op for ul_printk_char_out().
 * Linked when no platform provides a strong override.
 * On real hardware without a console, output is silently discarded.
 */

#include <kernel/include/ul_printk.h>

__attribute__((weak)) void ul_printk_char_out(char c)
{
	(void)c;
}
