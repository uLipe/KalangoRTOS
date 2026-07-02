/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * stub/printk_stub.c — DOCUMENTATION ONLY, not compiled.
 *
 * The board is responsible for providing ulmk_printk_char_out().
 * For boards without a console, provide a silent no-op:
 *
 * #include <kernel/include/ulmk_printk.h>
 * void ulmk_printk_char_out(char c) { (void)c; }
 */

/*
#include <kernel/include/ulmk_printk.h>

__attribute__((weak)) void ulmk_printk_char_out(char c)
{
	(void)c;
}
*/
