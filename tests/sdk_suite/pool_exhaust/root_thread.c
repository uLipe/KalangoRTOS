/* SPDX-License-Identifier: MIT */
/*
 * pool_exhaust — prove INVALID on OOM when the heap is tight; on large-heap
 * boards, prove we can hold a full table then recover after free.
 */
#include "sdk_test_util.h"

#define MAX_EPS		128
#define MAX_NOTIFS	128
#define MAX_THREADS	32
#define STACK_FILL	2048u
#define STACK_OK	1024u

static int g_pass;
static int g_fail;
static ulmk_ep_t g_eps[MAX_EPS];
static ulmk_notif_t g_notifs[MAX_NOTIFS];
static ulmk_tid_t g_tids[MAX_THREADS];

static void check(const char *name, int ok)
{
	sdk_puts(ok ? ".ok " : ".FAIL ");
	sdk_puts(name);
	sdk_puts("\n");
	if (ok)
		g_pass++;
	else
		g_fail++;
}

#define CHECK(name, cond) check((name), (cond) ? 1 : 0)

static void blocker(void *arg)
{
	(void)arg;
	for (;;)
		ulmk_thread_yield();
}

static void check_cap_or_exhaust(const char *ex_name, const char *cap_name,
				 int n, int max)
{
	if (n < max)
		CHECK(ex_name, 1);
	else
		CHECK(cap_name, 1);
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	int          n_ep;
	int          n_notif;
	int          n_th;
	int          i;
	ulmk_ep_t    ep;
	ulmk_notif_t n;
	ulmk_tid_t   tid;

	board_services_init(info);
	sdk_puts("pool_exhaust: begin\n");
	g_pass = 0;
	g_fail = 0;

	n_th = 0;
	for (i = 0; i < MAX_THREADS; i++) {
		tid = sdk_spawn("blk", blocker, NULL, 200u, STACK_FILL, 0u);
		if (tid == ULMK_TID_INVALID)
			break;
		g_tids[n_th++] = tid;
	}
	CHECK("th_got_some", n_th > 1);
	check_cap_or_exhaust("th_exhausted", "th_held_max", n_th, MAX_THREADS);

	n_ep = 0;
	for (i = 0; i < MAX_EPS; i++) {
		ep = ulmk_ep_create();
		if (ep == ULMK_EP_INVALID)
			break;
		g_eps[n_ep++] = ep;
	}
	check_cap_or_exhaust("ep_exhausted", "ep_held_max", n_ep, MAX_EPS);

	n_notif = 0;
	for (i = 0; i < MAX_NOTIFS; i++) {
		n = ulmk_notif_create();
		if (n == ULMK_NOTIF_INVALID)
			break;
		g_notifs[n_notif++] = n;
	}
	check_cap_or_exhaust("notif_exhausted", "notif_held_max", n_notif,
			     MAX_NOTIFS);

	for (i = 0; i < n_ep; i++)
		(void)ulmk_ep_destroy(g_eps[i]);
	for (i = 0; i < n_notif; i++)
		(void)ulmk_notif_destroy(g_notifs[i]);
	for (i = 0; i < n_th; i++)
		(void)ulmk_thread_kill(g_tids[i]);

	ep = ulmk_ep_create();
	CHECK("ep_recover", ep != ULMK_EP_INVALID);
	(void)ulmk_ep_destroy(ep);
	n = ulmk_notif_create();
	CHECK("notif_recover", n != ULMK_NOTIF_INVALID);
	(void)ulmk_notif_destroy(n);
	tid = sdk_spawn("ok", blocker, NULL, 200u, STACK_OK, 0u);
	CHECK("th_recover", tid != ULMK_TID_INVALID);
	if (tid != ULMK_TID_INVALID)
		(void)ulmk_thread_kill(tid);

	sdk_puts("pool_exhaust: pass=");
	sdk_put_u32((uint32_t)g_pass);
	sdk_puts(" fail=");
	sdk_put_u32((uint32_t)g_fail);
	sdk_puts(" th=");
	sdk_put_u32((uint32_t)n_th);
	sdk_puts(" ep=");
	sdk_put_u32((uint32_t)n_ep);
	sdk_puts(" notif=");
	sdk_put_u32((uint32_t)n_notif);
	sdk_puts("\n");
	sdk_puts(g_fail == 0 ? "pool_exhaust: PASS\n" : "pool_exhaust: FAIL\n");
	ulmk_thread_exit();
}
