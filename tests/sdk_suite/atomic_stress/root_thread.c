/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

#define CTX_YIELD_ITERS	100u
#define CTX_WORKERS	8

static volatile int g_ctx_done;

static void ctx_worker(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0u; i < CTX_YIELD_ITERS; i++)
		ulmk_thread_yield();
	g_ctx_done++;
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	int      i;
	uint32_t waited = 0u;

	(void)arg;
	sdk_puts("atomic_stress: start\n");
	g_ctx_done = 0;

	for (i = 0; i < CTX_WORKERS; i++)
		sdk_spawn("cw", ctx_worker, NULL, 15u, 2048u, 0u);

	while (g_ctx_done != CTX_WORKERS && waited < 40000u) {
		sdk_msleep_yield(1u);
		waited++;
	}

	if (g_ctx_done == CTX_WORKERS)
		sdk_puts("atomic_stress: PASS\n");
	else
		sdk_puts("atomic_stress: FAIL\n");
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;

	board_services_init(info);
	tid = sdk_spawn("sup", supervisor, NULL, 25u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_SPAWN);
	ulmk_thread_exit();
}
