/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Kernel printk — kernel/printk/ulmk_printk.c
 *
 * Self-contained format engine: no heap, no float, no <stdio.h>.
 * Output is routed one character at a time through ulmk_printk_char_out().
 *
 * Supported specifiers:
 *   %c   char
 *   %s   NUL-terminated string  (NULL → "(null)")
 *   %d   int32_t  signed decimal
 *   %i   int32_t  signed decimal (alias for %d)
 *   %u   uint32_t unsigned decimal
 *   %x   uint32_t hex lowercase
 *   %X   uint32_t hex uppercase
 *   %p   pointer  → "0x" + uintptr_t hex lowercase
 *   %lu  unsigned long  decimal
 *   %lx  unsigned long  hex lowercase
 *   %zu  size_t         decimal
 *   %%   literal '%'
 *
 * Width / precision / flags are not supported — keeps the engine < 150 LoC.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <ulmk/config.h>
#include <kernel/include/ulmk_printk.h>

#if ULMK_CONFIG_DEBUG_PRINTK

static const char hex_lower[] = "0123456789abcdef";
static const char hex_upper[] = "0123456789ABCDEF";

static void emit_str(const char *s)
{
	if (!s)
		s = "(null)";
	while (*s)
		ulmk_printk_char_out(*s++);
}

static void emit_u32_dec(uint32_t v)
{
	char buf[10];
	int i = 0;

	if (v == 0) {
		ulmk_printk_char_out('0');
		return;
	}
	while (v) {
		buf[i++] = '0' + (v % 10);
		v /= 10;
	}
	while (i--)
		ulmk_printk_char_out(buf[i]);
}

static void emit_i32_dec(int32_t v)
{
	if (v < 0) {
		ulmk_printk_char_out('-');
		/* avoid UB for INT32_MIN: cast through unsigned */
		emit_u32_dec((uint32_t)(-(v + 1)) + 1u);
	} else {
		emit_u32_dec((uint32_t)v);
	}
}

static void emit_u32_hex(uint32_t v, const char *digits)
{
	char buf[8];
	int i = 0;

	if (v == 0) {
		ulmk_printk_char_out('0');
		return;
	}
	while (v) {
		buf[i++] = digits[v & 0xf];
		v >>= 4;
	}
	while (i--)
		ulmk_printk_char_out(buf[i]);
}

static void emit_ulmk_dec(unsigned long v)
{
	char buf[20];
	int i = 0;

	if (v == 0) {
		ulmk_printk_char_out('0');
		return;
	}
	while (v) {
		buf[i++] = '0' + (int)(v % 10);
		v /= 10;
	}
	while (i--)
		ulmk_printk_char_out(buf[i]);
}

static void emit_ulmk_hex(unsigned long v, const char *digits)
{
	char buf[16];
	int i = 0;

	if (v == 0) {
		ulmk_printk_char_out('0');
		return;
	}
	while (v) {
		buf[i++] = digits[v & 0xf];
		v >>= 4;
	}
	while (i--)
		ulmk_printk_char_out(buf[i]);
}

void _ulmk_printk(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	while (*fmt) {
		if (*fmt != '%') {
			ulmk_printk_char_out(*fmt++);
			continue;
		}
		fmt++; /* consume '%' */

		/* Length modifier */
		int is_long = 0;
		if (*fmt == 'l') {
			is_long = 1;
			fmt++;
		} else if (*fmt == 'z') {
			is_long = (sizeof(size_t) > sizeof(uint32_t)) ? 1 : 0;
			fmt++;
		}

		switch (*fmt++) {
		case 'c':
			ulmk_printk_char_out((char)va_arg(ap, int));
			break;
		case 's':
			emit_str(va_arg(ap, const char *));
			break;
		case 'd':
		case 'i':
			if (is_long)
				emit_i32_dec((int32_t)va_arg(ap, long));
			else
				emit_i32_dec(va_arg(ap, int32_t));
			break;
		case 'u':
			if (is_long)
				emit_ulmk_dec(va_arg(ap, unsigned long));
			else
				emit_u32_dec(va_arg(ap, uint32_t));
			break;
		case 'x':
			if (is_long)
				emit_ulmk_hex(va_arg(ap, unsigned long), hex_lower);
			else
				emit_u32_hex(va_arg(ap, uint32_t), hex_lower);
			break;
		case 'X':
			if (is_long)
				emit_ulmk_hex(va_arg(ap, unsigned long), hex_upper);
			else
				emit_u32_hex(va_arg(ap, uint32_t), hex_upper);
			break;
		case 'p': {
			uintptr_t p = (uintptr_t)va_arg(ap, void *);
			ulmk_printk_char_out('0');
			ulmk_printk_char_out('x');
			emit_ulmk_hex((unsigned long)p, hex_lower);
			break;
		}
		case '%':
			ulmk_printk_char_out('%');
			break;
		default:
			ulmk_printk_char_out('?');
			break;
		}
	}

	va_end(ap);
}

#endif /* ULMK_CONFIG_DEBUG_PRINTK */
