/* SPDX-License-Identifier: MIT */
/*
 * kill_rendezvous — kill mid-IPC must not leave peers stuck.
 *   A) kill server after recv, before reply → caller gets ESRCH
 *   B) kill caller on send_queue → server recv stays clean
 *   C) kill server on recv_queue → ep reusable
 */
#include "sdk_test_util.h"

#define BIT_GO		(1u << 0)
#define BIT_HOLDING	(1u << 1)
#define BIT_DONE	(1u << 2)
#define BIT_PARKED	(1u << 3)

static int g_pass;
static int g_fail;
static ulmk_ep_t g_ep;
static ulmk_notif_t g_sync;
static volatile int g_call_rc;
static volatile int g_recv_rc;
static ulmk_tid_t g_server;
static ulmk_tid_t g_client;

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

/* --- A: server holds after recv; root kills; client unblocks --- */

static void srv_hold(void *arg)
{
	ulmk_msg_t m;
	ulmk_tid_t sender;
	uint32_t   bits = 0u;

	(void)arg;
	if (ulmk_ep_recv(g_ep, &m, &sender) != ULMK_OK)
		ulmk_thread_exit();
	ulmk_notif_signal(g_sync, BIT_HOLDING);
	ulmk_notif_wait(g_sync, BIT_PARKED, &bits); /* killed before wake */
	ulmk_thread_exit();
}

static void cli_call(void *arg)
{
	ulmk_msg_t m;
	uint32_t   bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	m.label = 0xAAu;
	g_call_rc = ulmk_ep_call(g_ep, &m);
	ulmk_notif_signal(g_sync, BIT_DONE);
	ulmk_thread_exit();
}

/* --- B: client parked on send_queue; kill client; server recv empty --- */

static void cli_park_call(void *arg)
{
	ulmk_msg_t m;

	(void)arg;
	m.label = 0xBBu;
	(void)ulmk_ep_call(g_ep, &m);
	ulmk_thread_exit();
}

static void srv_empty_recv(void *arg)
{
	ulmk_msg_t m;
	ulmk_tid_t sender;
	uint32_t   bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	g_recv_rc = ulmk_ep_recv(g_ep, &m, &sender);
	ulmk_notif_signal(g_sync, BIT_DONE);
	ulmk_thread_exit();
}

/* --- C: kill server on recv; new server + call works --- */

static void srv_park_recv(void *arg)
{
	ulmk_msg_t m;
	ulmk_tid_t sender;

	(void)arg;
	(void)ulmk_ep_recv(g_ep, &m, &sender);
	ulmk_thread_exit();
}

static void srv_ok(void *arg)
{
	ulmk_msg_t m;
	ulmk_tid_t sender;

	(void)arg;
	if (ulmk_ep_recv(g_ep, &m, &sender) != ULMK_OK)
		ulmk_thread_exit();
	m.label = 0xCCu;
	(void)ulmk_ep_reply(sender, &m);
	ulmk_thread_exit();
}

static void cli_ok(void *arg)
{
	ulmk_msg_t m;
	uint32_t   bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	m.label = 1u;
	g_call_rc = ulmk_ep_call(g_ep, &m);
	if (g_call_rc == ULMK_OK && m.label != 0xCCu)
		g_call_rc = ULMK_EINVAL;
	ulmk_notif_signal(g_sync, BIT_DONE);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	uint32_t bits = 0u;
	int      i;
	int      rc;

	board_services_init(info);
	sdk_puts("kill_rendezvous: begin\n");
	g_pass = 0;
	g_fail = 0;

	g_sync = ulmk_notif_create();
	CHECK("sync", g_sync != ULMK_NOTIF_INVALID);

	/* ----- case A ----- */
	g_ep = ulmk_ep_create();
	g_call_rc = 0;
	CHECK("A_ep", g_ep != ULMK_EP_INVALID);
	g_server = sdk_spawn("srvA", srv_hold, NULL, 50u, 1024u, 0u);
	g_client = sdk_spawn("cliA", cli_call, NULL, 10u, 1024u, 0u);
	CHECK("A_spawn", g_server != ULMK_TID_INVALID &&
			 g_client != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, g_server);
	(void)ulmk_ep_grant(g_ep, g_client);

	(void)ulmk_thread_priority_set(ulmk_thread_self(), 200u);
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();

	ulmk_notif_signal(g_sync, BIT_GO);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_HOLDING, &bits);
	rc = ulmk_thread_kill(g_server);
	CHECK("A_kill", rc == ULMK_OK);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_DONE, &bits);
	CHECK("A_caller_esrch", g_call_rc == ULMK_ESRCH);
	(void)ulmk_ep_destroy(g_ep);

	/* ----- case B ----- */
	g_ep = ulmk_ep_create();
	g_recv_rc = ULMK_OK;
	CHECK("B_ep", g_ep != ULMK_EP_INVALID);
	g_client = sdk_spawn("cliB", cli_park_call, NULL, 10u, 1024u, 0u);
	g_server = sdk_spawn("srvB", srv_empty_recv, NULL, 50u, 1024u, 0u);
	CHECK("B_spawn", g_server != ULMK_TID_INVALID &&
			 g_client != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, g_server);
	(void)ulmk_ep_grant(g_ep, g_client);

	for (i = 0; i < 8; i++)
		ulmk_thread_yield();

	rc = ulmk_thread_kill(g_client);
	CHECK("B_kill", rc == ULMK_OK);
	ulmk_notif_signal(g_sync, BIT_GO);
	/*
	 * Server was waiting for GO then recv. With empty send_queue it
	 * blocks — wake it by destroying the ep (EINVAL), proving no ghost.
	 */
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();
	rc = ulmk_ep_destroy(g_ep);
	CHECK("B_destroy", rc == ULMK_OK);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_DONE, &bits);
	CHECK("B_recv_einval", g_recv_rc == ULMK_EINVAL);

	/* ----- case C ----- */
	g_ep = ulmk_ep_create();
	g_call_rc = ULMK_EINVAL;
	CHECK("C_ep", g_ep != ULMK_EP_INVALID);
	g_server = sdk_spawn("srvC0", srv_park_recv, NULL, 50u, 1024u, 0u);
	CHECK("C_spawn0", g_server != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, g_server);
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();
	CHECK("C_kill0", ulmk_thread_kill(g_server) == ULMK_OK);

	g_server = sdk_spawn("srvC1", srv_ok, NULL, 50u, 1024u, 0u);
	g_client = sdk_spawn("cliC", cli_ok, NULL, 10u, 1024u, 0u);
	CHECK("C_spawn1", g_server != ULMK_TID_INVALID &&
			  g_client != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, g_server);
	(void)ulmk_ep_grant(g_ep, g_client);
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();
	ulmk_notif_signal(g_sync, BIT_GO);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_DONE, &bits);
	CHECK("C_call_ok", g_call_rc == ULMK_OK);
	(void)ulmk_ep_destroy(g_ep);

	sdk_puts("kill_rendezvous: pass=");
	sdk_put_u32((uint32_t)g_pass);
	sdk_puts(" fail=");
	sdk_put_u32((uint32_t)g_fail);
	sdk_puts("\n");
	sdk_puts(g_fail == 0 ? "kill_rendezvous: PASS\n"
			     : "kill_rendezvous: FAIL\n");
	ulmk_thread_exit();
}
