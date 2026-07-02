/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * QEMU TC3xx console — boards/qemu_tc3xx/qemu_console.c
 *
 * Linumiz QEMU-AURIX VIRT debug device (hw/tricore/tricore_virt.c):
 *   0xBF000020  uint32_t  write byte → putchar on QEMU stdout
 *   0xBF000028  uint32_t  write code → exit(code) inside QEMU
 *
 * Two separate interfaces:
 *
 *  1. Kernel hook — ul_printk_char_out() / ul_sim_exit()
 *     Called from kernel/supervisor context.  Uses direct MMIO access.
 *
 *  2. Userspace console — console_init() / console_puts() / console_printf()
 *     Called from driver-privilege threads.  Maps the device via
 *     ul_mem_map(UL_MMAP_PERIPH) and uses a self-contained format engine
 *     (no libc stdio dependency).
 */

#include <stdint.h>
#include <stdarg.h>
#include <ul/microkernel.h>
#include "console.h"

/*
 * This is the ONLY board file permitted to include ul_printk.h — it provides
 * the kernel hook ul_printk_char_out() and is compiled as part of the kernel
 * image, not as isolated userspace code.
 */
#include <kernel/include/ul_printk.h>

#define VIRT_BASE        0xBF000000UL
#define VIRT_REGION_SIZE 0x40U
#define VIRT_PUTCHAR_OFF 0x20U
#define VIRT_EXIT_OFF    0x28U

/* ── Kernel hook ──────────────────────────────────────────────────────────── */

void ul_printk_char_out(char c)
{
	volatile uint32_t *reg =
		(volatile uint32_t *)(VIRT_BASE + VIRT_PUTCHAR_OFF);
	*reg = (uint32_t)(uint8_t)c;
}

__attribute__((noreturn)) void ul_sim_exit(int code)
{
	volatile uint32_t *reg =
		(volatile uint32_t *)(VIRT_BASE + VIRT_EXIT_OFF);
	*reg = (uint32_t)code;
	for (;;)
		;
}

/* ── Userspace console ────────────────────────────────────────────────────── */

static volatile uint32_t *s_putchar_reg;

void console_init(void)
{
	uintptr_t base;

	base = (uintptr_t)ul_mem_map((void *)VIRT_BASE, VIRT_REGION_SIZE,
				     UL_PERM_READ | UL_PERM_WRITE,
				     UL_MMAP_PERIPH);
	s_putchar_reg = (volatile uint32_t *)(base + VIRT_PUTCHAR_OFF);
}

void console_puts(const char *s)
{
	if (!s_putchar_reg)
		return;
	if (!s)
		s = "(null)";
	while (*s)
		*s_putchar_reg = (uint32_t)(uint8_t)*s++;
}

/*
 * Self-contained format engine — no <stdio.h>, no heap, no libc I/O.
 * Supports: %c %s %d %i %u %x %X %p %lu %lx %zu %%
 */

static void emit_char(char c)
{
	if (s_putchar_reg)
		*s_putchar_reg = (uint32_t)(uint8_t)c;
}

static void emit_u32_dec(uint32_t v)
{
	char buf[10];
	int i = 0;

	if (v == 0) {
		emit_char('0');
		return;
	}
	while (v) {
		buf[i++] = (char)('0' + (v % 10));
		v /= 10;
	}
	while (i--)
		emit_char(buf[i]);
}

static void emit_i32_dec(int32_t v)
{
	if (v < 0) {
		emit_char('-');
		emit_u32_dec((uint32_t)(-(v + 1)) + 1u);
	} else {
		emit_u32_dec((uint32_t)v);
	}
}

static void emit_ul_dec(unsigned long v)
{
	char buf[20];
	int i = 0;

	if (v == 0) {
		emit_char('0');
		return;
	}
	while (v) {
		buf[i++] = (char)('0' + (int)(v % 10));
		v /= 10;
	}
	while (i--)
		emit_char(buf[i]);
}

static void emit_ul_hex(unsigned long v, int upper)
{
	static const char lo[] = "0123456789abcdef";
	static const char hi[] = "0123456789ABCDEF";
	const char *digits = upper ? hi : lo;
	char buf[16];
	int i = 0;

	if (v == 0) {
		emit_char('0');
		return;
	}
	while (v) {
		buf[i++] = digits[v & 0xf];
		v >>= 4;
	}
	while (i--)
		emit_char(buf[i]);
}

void console_printf(const char *fmt, ...)
{
	va_list ap;
	int is_long;

	va_start(ap, fmt);

	while (*fmt) {
		if (*fmt != '%') {
			emit_char(*fmt++);
			continue;
		}
		fmt++;

		is_long = 0;
		if (*fmt == 'l') {
			is_long = 1;
			fmt++;
		} else if (*fmt == 'z') {
			is_long = (sizeof(size_t) > sizeof(uint32_t)) ? 1 : 0;
			fmt++;
		}

		switch (*fmt++) {
		case 'c':
			emit_char((char)va_arg(ap, int));
			break;
		case 's':
			console_puts(va_arg(ap, const char *));
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
				emit_ul_dec(va_arg(ap, unsigned long));
			else
				emit_u32_dec(va_arg(ap, uint32_t));
			break;
		case 'x':
			if (is_long)
				emit_ul_hex(va_arg(ap, unsigned long), 0);
			else
				emit_ul_hex((unsigned long)va_arg(ap, uint32_t), 0);
			break;
		case 'X':
			if (is_long)
				emit_ul_hex(va_arg(ap, unsigned long), 1);
			else
				emit_ul_hex((unsigned long)va_arg(ap, uint32_t), 1);
			break;
		case 'p':
			emit_char('0');
			emit_char('x');
			emit_ul_hex((unsigned long)(uintptr_t)va_arg(ap, void *), 0);
			break;
		case '%':
			emit_char('%');
			break;
		default:
			emit_char('?');
			break;
		}
	}

	va_end(ap);
}
