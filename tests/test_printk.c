/* SPDX-License-Identifier: MIT */
/*
 * Userspace-safe printk for integration tests (lives in .user_text).
 * Kernel code must not be called directly once MPU CPR isolation is enabled.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#define VIRT_PUTCHAR  (0xBF000000UL + 0x20U)

static void test_putchar(char c)
{
	volatile uint32_t *reg = (volatile uint32_t *)VIRT_PUTCHAR;

	*reg = (uint32_t)(uint8_t)c;
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
	volatile uint32_t *reg = (volatile uint32_t *)(0xBF000000UL + 0x28U);

	*reg = (uint32_t)code;
	for (;;)
		;
}
