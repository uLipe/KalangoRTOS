/* SPDX-License-Identifier: MIT */
/*
 * RISC-V virt test I/O — UART @ 0x10000000, finisher @ 0x00100000.
 * Lives in .user_text; used instead of test_printk.c (TriCore 0xBF000020).
 */

#include <stdarg.h>
#include <stdint.h>

#define UART_BASE	0x10000000u
#define UART_LSR	5u
#define UART_LSR_TX_IDLE	(1u << 5)

#define FINISHER_BASE	0x00100000u
#define FINISHER_PASS	0x5555u

static void test_putchar(char c)
{
	volatile uint8_t *uart = (volatile uint8_t *)(uintptr_t)UART_BASE;

	while (!(uart[UART_LSR] & UART_LSR_TX_IDLE))
		;
	uart[0] = (uint8_t)c;
}

static void test_puts(const char *s)
{
	if (!s)
		s = "(null)";
	while (*s)
		test_putchar(*s++);
}

static void test_u32_dec(uint32_t v)
{
	char buf[10];
	int i = 0;

	if (v == 0u) {
		test_putchar('0');
		return;
	}
	while (v) {
		buf[i++] = (char)('0' + (v % 10u));
		v /= 10u;
	}
	while (i--)
		test_putchar(buf[i]);
}

static void test_i32_dec(int32_t v)
{
	if (v < 0) {
		test_putchar('-');
		test_u32_dec((uint32_t)(-(v + 1)) + 1u);
	} else {
		test_u32_dec((uint32_t)v);
	}
}

static void test_hex32(uint32_t v)
{
	static const char digits[] = "0123456789abcdef";
	char buf[8];
	int i = 0;

	if (v == 0u) {
		test_putchar('0');
		return;
	}
	while (v) {
		buf[i++] = digits[v & 0xfu];
		v >>= 4;
	}
	while (i--)
		test_putchar(buf[i]);
}

void test_printk(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	while (*fmt) {
		if (*fmt != '%') {
			test_putchar(*fmt++);
			continue;
		}
		fmt++;
		switch (*fmt++) {
		case 'c':
			test_putchar((char)va_arg(ap, int));
			break;
		case 's':
			test_puts(va_arg(ap, const char *));
			break;
		case 'd':
		case 'i':
			test_i32_dec(va_arg(ap, int32_t));
			break;
		case 'u':
			test_u32_dec(va_arg(ap, uint32_t));
			break;
		case 'l':
			if (*fmt == 'u') {
				fmt++;
				test_u32_dec((uint32_t)va_arg(ap, unsigned long));
			} else if (*fmt == 'x') {
				fmt++;
				test_hex32((uint32_t)va_arg(ap, unsigned long));
			} else {
				test_putchar('?');
			}
			break;
		case 'x':
			test_hex32(va_arg(ap, uint32_t));
			break;
		case 'p':
			test_putchar('0');
			test_putchar('x');
			test_hex32((uint32_t)(uintptr_t)va_arg(ap, void *));
			break;
		case '%':
			test_putchar('%');
			break;
		default:
			test_putchar('?');
			break;
		}
	}

	va_end(ap);
}

__attribute__((noreturn)) void ulmk_sim_exit(int code)
{
	volatile uint32_t *fin = (volatile uint32_t *)(uintptr_t)FINISHER_BASE;

	*fin = FINISHER_PASS | ((uint32_t)(code & 0xFF) << 16);
	for (;;)
		;
}
