/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * stub/printk_stub.c — DOCUMENTATION ONLY, not compiled.
 *
 * The board is responsible for providing ul_printk_char_out().
 * For boards without a console, provide a silent no-op:
 *
 * #include <kernel/include/ul_printk.h>
 * void ul_printk_char_out(char c) { (void)c; }
 */

/*
#include <kernel/include/ul_printk.h>

__attribute__((weak)) void ul_printk_char_out(char c)
{
	(void)c;
}
*/
