/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

#define SLEEP_MS	200u

static volatile int g_sleeper_rc;
static volatile int g_sleeper_started;

static void sleeper(void *arg)
{
	(void)arg;
	g_sleeper_started = 1;
	g_sleeper_rc = ulmk_sleep_ms(SLEEP_MS);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;
	int        rc;
	uint32_t   i;

	board_services_init(info);
	sdk_puts("sleep_cancel: start\n");

	g_sleeper_rc = 0x7fffffff;
	g_sleeper_started = 0;
	tid = sdk_spawn("slp", sleeper, NULL, 5u, 2048u, 0u);
	if (tid == ULMK_TID_INVALID) {
		sdk_puts("sleep_cancel: FAIL spawn\n");
		ulmk_thread_exit();
	}

	for (i = 0u; i < 10000u && !g_sleeper_started; i++)
		ulmk_thread_yield();

	/*
	 * g_sleeper_started is set before the sleep syscall — yield/sleep a
	 * tick so the sleeper is actually blocked before we cancel.
	 */
	(void)ulmk_sleep_ms(5u);

	rc = ulmk_sleep_cancel(tid);
	if (rc != ULMK_OK) {
		sdk_puts("sleep_cancel: FAIL cancel rc=\n");
		sdk_put_u32((uint32_t)rc);
		sdk_puts("\n");
		ulmk_thread_exit();
	}

	/*
	 * Root is prio 0 — yield alone never schedules the prio-5 sleeper.
	 * Block briefly so the cancelled sleeper can run and publish g_sleeper_rc.
	 */
	(void)ulmk_sleep_ms(5u);

	if (g_sleeper_rc != ULMK_ECANCELED) {
		sdk_puts("sleep_cancel: FAIL sleeper_rc=\n");
		sdk_put_u32((uint32_t)g_sleeper_rc);
		sdk_puts("\n");
		ulmk_thread_exit();
	}

	sdk_puts("sleep_cancel: PASS\n");
	ulmk_thread_exit();
}
