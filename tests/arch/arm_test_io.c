/* SPDX-License-Identifier: MIT */
/*
 * ARM Cortex-M test I/O — CMSDK APB UART (board_config BOARD_CONSOLE_UART_BASE)
 * plus ARM semihosting SYS_EXIT for the finisher.  Lives in .user_text; used
 * instead of test_printk.c (TriCore 0xBF000020) / riscv_test_io.c.
 *
 * The MMIO window is granted to user threads by the static peripheral MPU
 * region, so a userspace test may print directly.  QEMU must run with
 * -semihosting (see tests/arch/arm.mk) for ulmk_sim_exit() to stop the machine.
 */

#include <stdarg.h>
#include <stdint.h>
#include "board_config.h"

/* CMSDK APB UART register indices (32-bit words). */
#define UART_DATA		0u
#define UART_STATE		1u	/* bit0 = TX buffer full */
#define UART_CTRL		2u	/* bit0 = TX enable      */
#define UART_STATE_TXFULL	(1u << 0)
#define UART_CTRL_TXEN		(1u << 0)

static void test_putchar(char c)
{
	volatile uint32_t *uart =
		(volatile uint32_t *)(uintptr_t)BOARD_CONSOLE_UART_BASE;

	uart[UART_CTRL] = UART_CTRL_TXEN;
	while (uart[UART_STATE] & UART_STATE_TXFULL)
		;
	uart[UART_DATA] = (uint32_t)(uint8_t)c;
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

/*
 * ARM semihosting SYS_EXIT_EXTENDED (0x20): pass an application exit reason and
 * an exit code so QEMU (-semihosting) leaves with the test's status.
 */
#define SYS_EXIT_EXTENDED		0x20u
#define ADP_STOPPED_APPLICATION_EXIT	0x20026u

__attribute__((noreturn)) void ulmk_sim_exit(int code)
{
	uint32_t block[2];

	block[0] = ADP_STOPPED_APPLICATION_EXIT;
	block[1] = (uint32_t)code;

	{
		register uint32_t r0 __asm__("r0") = SYS_EXIT_EXTENDED;
		register uint32_t r1 __asm__("r1") = (uint32_t)(uintptr_t)block;

		__asm__ volatile("bkpt 0xAB"
				 : : "r"(r0), "r"(r1) : "memory");
	}

	for (;;)
		;
}
