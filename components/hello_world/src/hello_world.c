/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Hello world task — components/hello_world/src/hello_world.c
 *
 * Demonstrates the board console IPC pattern:
 *   - board_console_putc() / board_console_puts() are the public C API of
 *     the board_console service (defined in boards/<board>/board_console.h).
 *   - All output goes through IPC; no direct MMIO access from this component.
 *   - hello runs at priority 10; ping at 11 (must differ — equal priority on
 *     TriCore with a shared board-timer client triggers a scheduling bug).
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk/linker.h>
#include <hello_world.h>

/* board_console_putc and board_console_puts are resolved at link time from
 * the board sources.  Forward-declared here to avoid a board-specific include
 * inside a portable component. */
void board_console_putc(char c);
void board_console_puts(const char *s);
void board_timer_sleep_us(uint32_t us);

static void print_uint32(uint32_t v)
{
	char buf[11];
	int  i = (int)sizeof(buf) - 1;

	buf[i] = '\0';
	do {
		buf[--i] = (char)('0' + (int)(v % 10u));
		v /= 10u;
	} while (v && i > 0);
	board_console_puts(&buf[i]);
}

static void hello_entry(void *arg)
{
	uint32_t n = 0;

	(void)arg;

	for (;;) {
		board_console_puts("ulmk: hello from userspace — tick #");
		print_uint32(n++);
		board_console_putc('\n');

		board_timer_sleep_us(100000u);
	}
}

ulmk_tid_t hello_world_init(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t         tid;
	static ULMK_PRIVATE int done;

	(void)info;

	if (done)
		return ULMK_TID_INVALID;
	done = 1;

	attr.name       = "hello";
	attr.entry      = hello_entry;
	attr.arg        = NULL;
	attr.priority   = 10u;
	attr.stack_size = 2048u;
	attr.privilege  = ULMK_PRIV_USER;

	tid = ulmk_thread_create(&attr);
	return tid;
}
