/* SPDX-License-Identifier: MIT */
/*
 * Spawn/exit churn — sequential workers avoid lost ++ under preemption.
 */
#include "sdk_test_util.h"

#define TOTAL_THREADS	40

static volatile int g_worker_done;
static volatile int g_total_ok;

static void worker(void *arg)
{
	(void)arg;
	ulmk_thread_yield();
	g_worker_done = 1;
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	int i;

	(void)arg;
	sdk_puts("resource_leak: start\n");

	for (i = 0; i < TOTAL_THREADS; i++) {
		g_worker_done = 0;
		if (sdk_spawn("w", worker, NULL, 1u, 2048u, 0u)
		    == ULMK_TID_INVALID) {
			sdk_puts("resource_leak: spawn FAIL\n");
			sdk_puts("resource_leak: FAIL\n");
			ulmk_thread_exit();
		}
		while (!g_worker_done)
			ulmk_thread_yield();
		g_total_ok++;
	}

	if (g_total_ok == TOTAL_THREADS)
		sdk_puts("resource_leak: PASS\n");
	else
		sdk_puts("resource_leak: FAIL\n");
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;

	board_services_init(info);
	tid = sdk_spawn("sup", supervisor, NULL, 100u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_SPAWN);
	ulmk_thread_exit();
}
