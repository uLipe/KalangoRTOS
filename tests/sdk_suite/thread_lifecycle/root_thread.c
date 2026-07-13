/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

static volatile int g_a_done;
static volatile int g_b_done;
static volatile int g_c_done;
static volatile int g_kill_ran;

static void worker_a(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < 10; i++)
		ulmk_thread_yield();
	g_a_done = 1;
	ulmk_thread_exit();
}

static void worker_b(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < 10; i++)
		ulmk_thread_yield();
	g_b_done = 1;
	ulmk_thread_exit();
}

static void worker_c(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < 10; i++)
		ulmk_thread_yield();
	g_c_done = 1;
	ulmk_thread_exit();
}

static void worker_kill(void *arg)
{
	(void)arg;
	g_kill_ran = 1;
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	ulmk_tid_t tid_c;
	ulmk_tid_t tid_kill;
	int        prio;
	int        ok = 1;

	(void)arg;
	sdk_puts("thread_lifecycle: start\n");

	sdk_spawn("A", worker_a, NULL, 1u, 1024u, 0u);
	sdk_spawn("B", worker_b, NULL, 2u, 1024u, 0u);
	tid_c = sdk_spawn("C", worker_c, NULL, 3u, 1024u, 0u);
	ulmk_thread_suspend(tid_c);

	/* Lowest urgency so create→kill cannot be preempted into K. */
	tid_kill = sdk_spawn("K", worker_kill, NULL, 200u, 1024u, 0u);
	ulmk_thread_kill(tid_kill);

	while (!g_a_done || !g_b_done)
		ulmk_thread_yield();
	sdk_puts("thread_lifecycle: A done\n");
	sdk_puts("thread_lifecycle: B done\n");

	ulmk_thread_resume(tid_c);
	while (!g_c_done)
		ulmk_thread_yield();
	sdk_puts("thread_lifecycle: C resumed\n");
	sdk_puts("thread_lifecycle: C done\n");

	if (g_kill_ran) {
		sdk_puts("thread_lifecycle: FAIL kill target ran!\n");
		ok = 0;
	} else {
		sdk_puts("thread_lifecycle: kill target never ran\n");
	}

	ulmk_thread_priority_set(ulmk_thread_self(), 200);
	prio = ulmk_thread_priority_get(ulmk_thread_self());
	if (prio != 200) {
		sdk_puts("thread_lifecycle: FAIL prio\n");
		ok = 0;
	}

	if (ok)
		sdk_puts("thread_lifecycle: PASS\n");
	else
		sdk_puts("thread_lifecycle: FAIL\n");
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;

	board_services_init(info);
	tid = sdk_spawn("sup", supervisor, NULL, 10u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_SPAWN);
	ulmk_thread_exit();
}
