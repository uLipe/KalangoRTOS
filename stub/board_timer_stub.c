/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * stub/board_timer_stub.c — weak no-op board timer for builds without a board.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>

__attribute__((weak)) ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info)
{
	(void)info;
	return ULMK_TID_INVALID;
}

__attribute__((weak)) void board_timer_sleep_us(uint32_t us)
{
	(void)us;
}
