/* SPDX-License-Identifier: MIT */
/*
 * destroy_waiters — ep/notif destroy unblocks waiters with ULMK_EINVAL.
 */
#include "sdk_test_util.h"

#define BIT_DONE	(1u << 0)

static int g_pass;
static int g_fail;
static ulmk_ep_t g_ep;
static ulmk_notif_t g_sync;
static ulmk_notif_t g_target;
static volatile int g_recv_rc;
static volatile int g_call_rc;
static volatile int g_wait_rc;

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

static void recv_waiter(void *arg)
{
	ulmk_msg_t m;
	ulmk_tid_t sender;

	(void)arg;
	g_recv_rc = ulmk_ep_recv(g_ep, &m, &sender);
	ulmk_notif_signal(g_sync, BIT_DONE);
	ulmk_thread_exit();
}

static void call_waiter(void *arg)
{
	ulmk_msg_t m;

	(void)arg;
	m.label = 1u;
	g_call_rc = ulmk_ep_call(g_ep, &m);
	ulmk_notif_signal(g_sync, BIT_DONE);
	ulmk_thread_exit();
}

static void notif_waiter(void *arg)
{
	uint32_t bits = 0u;

	(void)arg;
	g_wait_rc = ulmk_notif_wait(g_target, 0x1u, &bits);
	ulmk_notif_signal(g_sync, BIT_DONE);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;
	uint32_t   bits = 0u;
	int        i;
	int        rc;

	board_services_init(info);
	sdk_puts("destroy_waiters: begin\n");
	g_pass = 0;
	g_fail = 0;
	g_recv_rc = 0;
	g_call_rc = 0;
	g_wait_rc = 0;

	g_sync = ulmk_notif_create();
	CHECK("sync", g_sync != ULMK_NOTIF_INVALID);

	/* --- ep_recv waiter --- */
	g_ep = ulmk_ep_create();
	CHECK("ep_recv_create", g_ep != ULMK_EP_INVALID);
	tid = sdk_spawn("recv_w", recv_waiter, NULL, 10u, 1024u, 0u);
	CHECK("spawn_recv", tid != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, tid);

	(void)ulmk_thread_priority_set(ulmk_thread_self(), 200u);
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();

	rc = ulmk_ep_destroy(g_ep);
	CHECK("ep_destroy_recv", rc == ULMK_OK);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_DONE, &bits);
	CHECK("recv_einval", g_recv_rc == ULMK_EINVAL);

	/* --- ep_call waiter (no server) --- */
	g_ep = ulmk_ep_create();
	CHECK("ep_call_create", g_ep != ULMK_EP_INVALID);
	tid = sdk_spawn("call_w", call_waiter, NULL, 10u, 1024u, 0u);
	CHECK("spawn_call", tid != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, tid);

	for (i = 0; i < 8; i++)
		ulmk_thread_yield();

	rc = ulmk_ep_destroy(g_ep);
	CHECK("ep_destroy_call", rc == ULMK_OK);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_DONE, &bits);
	CHECK("call_einval", g_call_rc == ULMK_EINVAL);

	/* --- notif_wait waiter --- */
	g_target = ulmk_notif_create();
	CHECK("notif_create", g_target != ULMK_NOTIF_INVALID);
	tid = sdk_spawn("notif_w", notif_waiter, NULL, 10u, 1024u, 0u);
	CHECK("spawn_notif", tid != ULMK_TID_INVALID);

	for (i = 0; i < 8; i++)
		ulmk_thread_yield();

	rc = ulmk_notif_destroy(g_target);
	CHECK("notif_destroy", rc == ULMK_OK);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_DONE, &bits);
	CHECK("wait_einval", g_wait_rc == ULMK_EINVAL);

	sdk_puts("destroy_waiters: pass=");
	sdk_put_u32((uint32_t)g_pass);
	sdk_puts(" fail=");
	sdk_put_u32((uint32_t)g_fail);
	sdk_puts("\n");
	sdk_puts(g_fail == 0 ? "destroy_waiters: PASS\n" : "destroy_waiters: FAIL\n");
	ulmk_thread_exit();
}
