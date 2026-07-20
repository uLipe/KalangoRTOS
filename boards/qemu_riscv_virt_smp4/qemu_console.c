/* SPDX-License-Identifier: MIT */
/* boards/qemu_riscv_virt/qemu_console.c — userspace printf helpers */

#include <stdint.h>
#include <stdarg.h>
#include <ulmk/microkernel.h>
#include "console.h"
#include "board_console.h"

void console_init(void)
{
}

static void emit_char(char c)
{
	board_console_putc(c);
}

void console_puts(const char *s)
{
	board_console_puts(s);
}

static void emit_u32_dec(uint32_t v)
{
	char buf[10];
	int  i = 0;

	if (v == 0u) {
		emit_char('0');
		return;
	}
	while (v) {
		buf[i++] = (char)('0' + (v % 10u));
		v /= 10u;
	}
	while (i--)
		emit_char(buf[i]);
}

static void emit_hex32(uint32_t v)
{
	int i;

	emit_char('0');
	emit_char('x');
	for (i = 28; i >= 0; i -= 4)
		emit_char("0123456789abcdef"[(v >> i) & 0xFu]);
}

void console_printf(const char *fmt, ...)
{
	va_list ap;
	const char *p;
	uint32_t v;

	va_start(ap, fmt);
	for (p = fmt; *p; p++) {
		if (*p != '%') {
			emit_char(*p);
			continue;
		}
		p++;
		switch (*p) {
		case 's':
			console_puts(va_arg(ap, const char *));
			break;
		case 'c':
			emit_char((char)va_arg(ap, int));
			break;
		case 'd':
		case 'i':
			v = (uint32_t)va_arg(ap, int);
			if ((int32_t)v < 0) {
				emit_char('-');
				v = (uint32_t)(-(int32_t)v);
			}
			emit_u32_dec(v);
			break;
		case 'u':
			emit_u32_dec(va_arg(ap, uint32_t));
			break;
		case 'x':
		case 'p':
			emit_hex32(*p == 'p' ?
				   (uint32_t)(uintptr_t)va_arg(ap, void *) :
				   va_arg(ap, uint32_t));
			break;
		case '%':
			emit_char('%');
			break;
		default:
			emit_char('%');
			emit_char(*p);
			break;
		}
	}
	va_end(ap);
}
