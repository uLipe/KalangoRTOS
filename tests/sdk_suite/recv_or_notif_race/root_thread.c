/* SPDX-License-Identifier: MIT */
/*
 * recv_or_notif_race — both IPC and notif paths; concurrent wake is clean.
 */
#include "sdk_test_util.h"

#define BIT_GO		(1u << 0)
#define BIT_DONE	(1u << 1)
#define BIT_EVT		(1u << 0)

static int g_pass;
static int g_fail;
static ulmk_ep_t g_ep;
static ulmk_notif_t g_evt;
static ulmk_notif_t g_sync;
static volatile int g_rc;
static volatile uint32_t g_bits;
static volatile ulmk_tid_t g_sender;
static volatile uint32_t g_label;

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

static void server_ron(void *arg)
{
	ulmk_msg_t m;
	ulmk_tid_t sender;
	uint32_t   bits = 0u;
	int        rc;

	(void)arg;
	rc = ulmk_ep_recv_or_notif(g_ep, g_evt, BIT_EVT, &m, &sender, &bits);
	g_rc = rc;
	g_bits = bits;
	g_sender = sender;
	g_label = m.label;
	if (rc == ULMK_OK && sender != ULMK_TID_INVALID) {
		m.label = 0x90u;
		(void)ulmk_ep_reply(sender, &m);
	}
	ulmk_notif_signal(g_sync, BIT_DONE);
	ulmk_thread_exit();
}

static void client_call(void *arg)
{
	ulmk_msg_t m;
	uint32_t   bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	m.label = 0x42u;
	(void)ulmk_ep_call(g_ep, &m);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t srv;
	ulmk_tid_t cli;
	uint32_t   bits = 0u;
	int        i;
	ulmk_msg_t m;
	ulmk_tid_t sender;
	uint32_t   nb = 0u;
	int        rc;

	board_services_init(info);
	sdk_puts("recv_or_notif_race: begin\n");
	g_pass = 0;
	g_fail = 0;

	g_sync = ulmk_notif_create();
	CHECK("sync", g_sync != ULMK_NOTIF_INVALID);

	/* --- notif-fast path (bits already set) --- */
	g_ep = ulmk_ep_create();
	g_evt = ulmk_notif_create();
	CHECK("nf_objs", g_ep != ULMK_EP_INVALID &&
			 g_evt != ULMK_NOTIF_INVALID);
	(void)ulmk_notif_signal(g_evt, BIT_EVT);
	rc = ulmk_ep_recv_or_notif(g_ep, g_evt, BIT_EVT, &m, &sender, &nb);
	CHECK("notif_fast", rc == 1 && nb == BIT_EVT &&
			    sender == ULMK_TID_INVALID);
	(void)ulmk_notif_destroy(g_evt);
	(void)ulmk_ep_destroy(g_ep);

	/* --- IPC path: client call wakes blocked server --- */
	g_ep = ulmk_ep_create();
	g_evt = ulmk_notif_create();
	g_rc = -99;
	g_bits = 0;
	g_sender = ULMK_TID_INVALID;
	CHECK("ipc_objs", g_ep != ULMK_EP_INVALID &&
			  g_evt != ULMK_NOTIF_INVALID);
	srv = sdk_spawn("srv", server_ron, NULL, 50u, 1024u, 0u);
	cli = sdk_spawn("cli", client_call, NULL, 10u, 1024u, 0u);
	CHECK("ipc_spawn", srv != ULMK_TID_INVALID &&
			   cli != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, srv);
	(void)ulmk_ep_grant(g_ep, cli);
	(void)ulmk_thread_priority_set(ulmk_thread_self(), 200u);
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();
	ulmk_notif_signal(g_sync, BIT_GO);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_DONE, &bits);
	CHECK("ipc_path", g_rc == ULMK_OK && g_sender != ULMK_TID_INVALID &&
			  g_label == 0x42u);
	(void)ulmk_ep_destroy(g_ep);
	(void)ulmk_notif_destroy(g_evt);

	/* --- race: signal + call while server blocked; either path OK --- */
	g_ep = ulmk_ep_create();
	g_evt = ulmk_notif_create();
	g_rc = -99;
	g_bits = 0;
	g_sender = ULMK_TID_INVALID;
	CHECK("race_objs", g_ep != ULMK_EP_INVALID &&
			   g_evt != ULMK_NOTIF_INVALID);
	srv = sdk_spawn("srvR", server_ron, NULL, 50u, 1024u, 0u);
	cli = sdk_spawn("cliR", client_call, NULL, 10u, 1024u, 0u);
	CHECK("race_spawn", srv != ULMK_TID_INVALID &&
			    cli != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, srv);
	(void)ulmk_ep_grant(g_ep, cli);
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();
	ulmk_notif_signal(g_sync, BIT_GO);
	ulmk_notif_signal(g_evt, BIT_EVT);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_DONE, &bits);
	CHECK("race_clean",
	      (g_rc == 1 && g_bits == BIT_EVT) ||
	      (g_rc == ULMK_OK && g_sender != ULMK_TID_INVALID));
	(void)ulmk_ep_destroy(g_ep);
	(void)ulmk_notif_destroy(g_evt);

	sdk_puts("recv_or_notif_race: pass=");
	sdk_put_u32((uint32_t)g_pass);
	sdk_puts(" fail=");
	sdk_put_u32((uint32_t)g_fail);
	sdk_puts("\n");
	sdk_puts(g_fail == 0 ? "recv_or_notif_race: PASS\n"
			     : "recv_or_notif_race: FAIL\n");
	ulmk_thread_exit();
}
