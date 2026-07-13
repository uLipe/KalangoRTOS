/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

static volatile int g_preempt_seen;
static volatile int g_stop;
static ulmk_notif_t g_wake_notif;
static ulmk_notif_t g_done_notif;

#define BIT_WAKE	(1u << 0)
#define BIT_DONE	(1u << 1)

static void worker(void *arg)
{
	(void)arg;
	while (!g_stop)
		ulmk_thread_yield();
	ulmk_thread_exit();
}

static void high_prio_waiter(void *arg)
{
	uint32_t bits;

	(void)arg;
	ulmk_notif_wait(g_wake_notif, BIT_WAKE, &bits);
	g_preempt_seen = 1;
	ulmk_notif_signal(g_done_notif, BIT_DONE);
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	uint32_t bits;

	(void)arg;
	sdk_puts("preempt: start\n");

	g_wake_notif = ulmk_notif_create();
	g_done_notif = ulmk_notif_create();

	sdk_spawn("worker_a", worker, NULL, 20u, 1024u, 0u);
	sdk_spawn("worker_b", worker, NULL, 20u, 1024u, 0u);
	sdk_spawn("waiter", high_prio_waiter, NULL, 5u, 1024u, 0u);

	sdk_puts("preempt: signalling waiter\n");
	ulmk_notif_signal(g_wake_notif, BIT_WAKE);

	bits = 0u;
	ulmk_notif_wait(g_done_notif, BIT_DONE, &bits);
	g_stop = 1;

	if (g_preempt_seen)
		sdk_puts("preempt: PASS\n");
	else
		sdk_puts("preempt: FAIL\n");
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;

	board_services_init(info);
	tid = sdk_spawn("supervisor", supervisor, NULL, 15u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_SPAWN);
	ulmk_thread_exit();
}
