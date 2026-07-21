/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

#define SLEEP_MS	10u
#define SLEEP_COUNT	3u

static volatile uint32_t g_done_mask;

static void sleeper(void *arg)
{
	unsigned i;
	uint32_t bit = (uint32_t)(uintptr_t)arg;

	for (i = 0; i < SLEEP_COUNT; i++)
		(void)ulmk_sleep_ms(SLEEP_MS);

	g_done_mask |= bit;
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	(void)arg;
	while ((g_done_mask & 0x7u) != 0x7u)
		ulmk_thread_yield();

	sdk_puts("sleep: PASS\n");
	sdk_puts("sleep: thread A done\n");
	sdk_puts("sleep: thread B done\n");
	sdk_puts("sleep: thread C done\n");
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	board_services_init(info);
	sdk_puts("sleep: start\n");

	sdk_spawn("A", sleeper, (void *)(uintptr_t)0x1u, 5u, 2048u, 0u);
	sdk_spawn("B", sleeper, (void *)(uintptr_t)0x2u, 5u, 2048u, 0u);
	sdk_spawn("C", sleeper, (void *)(uintptr_t)0x4u, 5u, 2048u, 0u);
	sdk_spawn("sup", supervisor, NULL, 6u, 2048u, 0u);
	ulmk_thread_exit();
}
