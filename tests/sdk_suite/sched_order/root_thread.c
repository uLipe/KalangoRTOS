/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

static void high_thread_entry(void *arg)
{
	(void)arg;
	sdk_puts("sched_order: high step 1\n");
	ulmk_thread_yield();
	sdk_puts("sched_order: high step 2\n");
	ulmk_thread_suspend(ulmk_thread_self());
	for (;;)
		;
}

static void mid_thread_entry(void *arg)
{
	(void)arg;
	sdk_puts("sched_order: mid step 1\n");
	ulmk_thread_yield();
	sdk_puts("sched_order: mid step 2\n");
	ulmk_thread_suspend(ulmk_thread_self());
	for (;;)
		;
}

static void low_thread_entry(void *arg)
{
	(void)arg;
	sdk_puts("sched_order: low step 1\n");
	ulmk_thread_suspend(ulmk_thread_self());
	for (;;)
		;
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	board_services_init(info);
	sdk_puts("sched_order: start\n");

	ulmk_thread_priority_set(ulmk_thread_self(), 200);

	sdk_spawn("high", high_thread_entry, NULL, 1u, 1024u, 0u);
	sdk_spawn("mid", mid_thread_entry, NULL, 2u, 1024u, 0u);
	sdk_spawn("low", low_thread_entry, NULL, 3u, 1024u, 0u);

	ulmk_thread_yield();

	sdk_puts("sched_order: PASS\n");
	ulmk_thread_exit();
}
