/* SPDX-License-Identifier: MIT */
/*
 * Shared helpers for SDK suite cases — public API only.
 */

#ifndef SDK_TEST_UTIL_H
#define SDK_TEST_UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <board_services.h>
#include <board_console.h>

static inline int sdk_map_ok(const void *p)
{
	uintptr_t u = (uintptr_t)p;

	if (u == 0u)
		return 0;
	if (u >= 0x80000000u)
		return 1;
	return (intptr_t)u > 0;
}

static inline void sdk_puts(const char *s)
{
	board_console_puts(s);
}

static inline void sdk_put_u32(uint32_t v)
{
	char buf[10];
	int  i = 0;

	if (v == 0u) {
		board_console_putc('0');
		return;
	}
	while (v) {
		buf[i++] = (char)('0' + (v % 10u));
		v /= 10u;
	}
	while (i--)
		board_console_putc(buf[i]);
}

static inline void sdk_msleep_yield(uint32_t ms)
{
	uint32_t i;

	for (i = 0u; i < ms * 20u; i++)
		ulmk_thread_yield();
}

static inline ulmk_tid_t sdk_spawn_priv(const char *name, void (*entry)(void *),
					void *arg, uint8_t prio, size_t stack,
					size_t heap, ulmk_privilege_t priv)
{
	ulmk_thread_attr_t a = {0};

	a.name       = name;
	a.entry      = entry;
	a.arg        = arg;
	a.priority   = prio;
	a.stack_size = stack;
	a.privilege  = priv;
	a.heap_size  = heap;
	a.cpu        = 0u;
	return ulmk_thread_create(&a);
}

static inline ulmk_tid_t sdk_spawn(const char *name, void (*entry)(void *),
				   void *arg, uint8_t prio, size_t stack,
				   size_t heap)
{
	return sdk_spawn_priv(name, entry, arg, prio, stack, heap,
			      ULMK_PRIV_DRIVER);
}

#endif /* SDK_TEST_UTIL_H */
