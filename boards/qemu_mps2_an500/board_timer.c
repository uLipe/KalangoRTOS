/* SPDX-License-Identifier: MIT */
/*
 * Board timer — thin wrapper over ulmk_sleep_ms (kernel timing wheel).
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include "board_timer.h"

void board_timer_sleep_us(uint32_t us)
{
	uint32_t ms = (us + 999u) / 1000u;

	if (ms == 0u)
		ms = 1u;
	(void)ulmk_sleep_ms(ms);
}

ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info)
{
	(void)info;
	ulmk_tick_start();
	return (ulmk_tid_t)1u;
}
