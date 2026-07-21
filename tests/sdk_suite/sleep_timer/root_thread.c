/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"
#include <board_timer.h>

#define SLEEP_COUNT	3u
#define SLEEP_US	10000u

static volatile uint32_t g_done_mask;

static void sleeper(void *arg)
{
	unsigned i;
	uint32_t bit = (uint32_t)(uintptr_t)arg;

	for (i = 0; i < SLEEP_COUNT; i++)
		board_timer_sleep_us(SLEEP_US);

	g_done_mask |= bit;
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	(void)arg;
	while ((g_done_mask & 0x7u) != 0x7u)
		ulmk_thread_yield();

	sdk_puts("sleep_timer: thread A done\n");
	sdk_puts("sleep_timer: thread B done\n");
	sdk_puts("sleep_timer: thread C done\n");
	sdk_puts("sleep_timer: PASS\n");
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	board_services_init(info);
	sdk_puts("sleep_timer: start\n");
	sdk_puts("sleep_timer: thread A alive (sleep 10)\n");

	sdk_spawn("A", sleeper, (void *)(uintptr_t)0x1u, 5u, 2048u, 0u);
	sdk_spawn("B", sleeper, (void *)(uintptr_t)0x2u, 5u, 2048u, 0u);
	sdk_spawn("C", sleeper, (void *)(uintptr_t)0x4u, 5u, 2048u, 0u);
	sdk_spawn("sup", supervisor, NULL, 6u, 2048u, 0u);
	ulmk_thread_exit();
}
