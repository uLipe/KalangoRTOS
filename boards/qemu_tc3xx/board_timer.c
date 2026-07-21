/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Board timer — thin wrapper over kernel sleep (ulmk_sleep_ms).
 * Arms the kernel tick after console IPC is ready.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include "board_config.h"
#include "board_timer.h"

#ifndef ULMK_BOARD_TICK_CLOCK_HZ
#define ULMK_BOARD_TICK_CLOCK_HZ	50000000u
#endif

void board_timer_sleep_us(uint32_t us)
{
	uint32_t ms = (us + 999u) / 1000u;

	if (ms == 0u)
		ms = 1u;
	(void)ulmk_sleep_ms(ms);
}

uint32_t board_timer_now_ticks(void)
{
	return 0u;
}

uint32_t board_timer_ticks_to_ns(uint32_t dt)
{
	uint64_t ns;

	ns = ((uint64_t)dt * 1000000000ull) / (uint64_t)ULMK_BOARD_TICK_CLOCK_HZ;
	if (ns > 0xFFFFFFFFu)
		return 0xFFFFFFFFu;
	return (uint32_t)ns;
}

ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info)
{
	(void)info;
	ulmk_tick_start();
	return (ulmk_tid_t)1u;
}
