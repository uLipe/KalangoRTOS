/* SPDX-License-Identifier: MIT */
/* boards/qemu_mps2_an500/board_console.c */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include "board_console.h"
#include "board_internal.h"

#define CONSOLE_MSG_PUTC	1u

void board_console_putc(char c)
{
	ulmk_msg_t msg;

	msg.label    = CONSOLE_MSG_PUTC;
	msg.words[0] = (uint32_t)(uint8_t)c;
	ulmk_ep_call(board_service_ep(), &msg);
}

void board_console_puts(const char *s)
{
	if (!s)
		return;
	while (*s)
		board_console_putc(*s++);
}

ulmk_tid_t board_console_start(const ulmk_boot_info_t *info)
{
	(void)info;
	return ULMK_TID_INVALID;
}
