/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Board console service — boards/qemu_tc3xx/board_console.c
 *
 * Provides character output via an IPC server thread that owns the MMIO
 * mapping.  Callers use board_console_putc() / board_console_puts() from
 * any thread without direct MMIO access.
 *
 * Endpoint lifecycle:
 *   board_console_start() creates the endpoint and stores it in g_ep,
 *   then spawns the server thread.  Because the root thread runs at priority
 *   0 and the server at priority 1, the root thread completes initialisation
 *   before the server executes — g_ep is valid by the time the server calls
 *   ul_ep_recv(), and by the time any client calls board_console_putc().
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include "board_console.h"

#define VIRT_BASE        0xBF000000UL
#define VIRT_REGION_SIZE 0x40U
#define VIRT_PUTCHAR_OFF 0x20U

#define CONSOLE_MSG_PUTC  1u

static ul_ep_t g_ep;

/* ---- client API --------------------------------------------------------- */

void board_console_putc(char c)
{
	ul_msg_t msg;

	msg.label    = CONSOLE_MSG_PUTC;
	msg.words[0] = (uint32_t)(uint8_t)c;
	ul_ep_call(g_ep, &msg);
}

void board_console_puts(const char *s)
{
	if (!s)
		return;
	while (*s)
		board_console_putc(*s++);
}

/* ---- server thread ------------------------------------------------------ */

static void console_server(void *arg)
{
	volatile uint32_t *virt;
	ul_msg_t           msg;
	ul_tid_t           sender;

	(void)arg;

	virt = (volatile uint32_t *)ul_mem_map(
		(void *)VIRT_BASE,
		VIRT_REGION_SIZE,
		UL_PERM_READ | UL_PERM_WRITE,
		UL_MMAP_PERIPH);

	for (;;) {
		ul_ep_recv(g_ep, &msg, &sender);
		if (msg.label == CONSOLE_MSG_PUTC)
			*(virt + (VIRT_PUTCHAR_OFF / sizeof(uint32_t))) =
				(uint32_t)(uint8_t)msg.words[0];
		ul_ep_reply(sender, &(ul_msg_t){0});
	}
}

/* ---- init --------------------------------------------------------------- */

ul_tid_t board_console_start(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;
	ul_tid_t         tid;

	(void)info;

	g_ep = ul_ep_create();

	attr.name       = "bcon";
	attr.entry      = console_server;
	attr.arg        = NULL;
	attr.priority   = 1u;
	attr.stack_size = 1024u;
	attr.privilege  = UL_PRIV_DRIVER;

	tid = ul_thread_create(&attr);
	ul_cap_grant(tid, UL_CAP_MAP_PERIPH);
	return tid;
}
