/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Board services — boards/qemu_tc3xx/board_services.c
 *
 * Provides the two mandatory board entry points for the QEMU TC3xx target:
 *
 *   ulmk_board_init()        — called from ulmk_kern_start() before .data copy;
 *                            no globals, no kernel API allowed.
 *
 *   board_services_init()  — called from ulmk_root_thread() after the
 *                            kernel is fully initialised; spawns
 *                            background service threads.
 */

#include <ulmk/microkernel.h>
#include "board_services.h"
#include "board_console.h"
#include "board_timer.h"

void ulmk_board_init(void)
{
	/* QEMU needs no early hardware setup. */
}

void board_services_init(const ulmk_boot_info_t *info)
{
	board_console_start(info);
	board_timer_start(info);
}
