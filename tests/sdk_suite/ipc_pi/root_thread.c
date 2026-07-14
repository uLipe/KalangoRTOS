/* SPDX-License-Identifier: MIT */
/*
 * ipc_pi — priority inheritance on ep_call (sdk_suite).
 * Server observes boosted prio while handling; restored after reply.
 */
#include "sdk_test_util.h"

#define PRIO_SERVER		100u
#define PRIO_CLIENT		10u
#define BIT_GO			(1u << 0)
#define BIT_CLIENT_DONE		(1u << 1)
#define BIT_SRV_HOLD		(1u << 2)

static int g_pass;
static int g_fail;
static ulmk_ep_t g_ep;
static ulmk_notif_t g_sync;
static volatile int g_prio_during;
static volatile int g_call_ok;
static ulmk_tid_t g_server;

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

static void server_entry(void *arg)
{
	ulmk_msg_t m;
	ulmk_tid_t sender;
	uint32_t   bits = 0u;

	(void)arg;
	if (ulmk_ep_recv(g_ep, &m, &sender) != ULMK_OK)
		ulmk_thread_exit();
	g_prio_during = ulmk_thread_priority_get(ulmk_thread_self());
	m.label = 0xA11u;
	(void)ulmk_ep_reply(sender, &m);
	ulmk_notif_wait(g_sync, BIT_SRV_HOLD, &bits);
	ulmk_thread_exit();
}

static void client_entry(void *arg)
{
	ulmk_msg_t m;
	uint32_t   bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	m.label = 0x42u;
	g_call_ok = (ulmk_ep_call(g_ep, &m) == ULMK_OK && m.label == 0xA11u);
	ulmk_notif_signal(g_sync, BIT_CLIENT_DONE);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t client;
	uint32_t   bits = 0u;
	int        i;
	int        prio;

	board_services_init(info);
	sdk_puts("ipc_pi: begin\n");
	g_pass = 0;
	g_fail = 0;
	g_prio_during = -1;
	g_call_ok = 0;

	g_ep = ulmk_ep_create();
	g_sync = ulmk_notif_create();
	CHECK("ep", g_ep != ULMK_EP_INVALID);
	CHECK("notif", g_sync != ULMK_NOTIF_INVALID);

	g_server = sdk_spawn("srv", server_entry, NULL, PRIO_SERVER, 1024u, 0u);
	client = sdk_spawn("cli", client_entry, NULL, PRIO_CLIENT, 1024u, 0u);
	CHECK("spawn_srv", g_server != ULMK_TID_INVALID);
	CHECK("spawn_cli", client != ULMK_TID_INVALID);
	(void)ulmk_ep_grant(g_ep, g_server);
	(void)ulmk_ep_grant(g_ep, client);

	/*
	 * Root boots at prio 0; yield alone never runs lower-prio threads.
	 * Drop so client blocks on GO and server parks on ep_recv first.
	 */
	(void)ulmk_thread_priority_set(ulmk_thread_self(), 200u);
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();

	ulmk_notif_signal(g_sync, BIT_GO);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_CLIENT_DONE, &bits);

	CHECK("call", g_call_ok);
	CHECK("boosted", g_prio_during == (int)PRIO_CLIENT);

	prio = ulmk_thread_priority_get(g_server);
	CHECK("restored", prio == (int)PRIO_SERVER);

	ulmk_notif_signal(g_sync, BIT_SRV_HOLD);

	sdk_puts("ipc_pi: pass=");
	sdk_put_u32((uint32_t)g_pass);
	sdk_puts(" fail=");
	sdk_put_u32((uint32_t)g_fail);
	sdk_puts(" prio_during=");
	sdk_put_u32((uint32_t)(g_prio_during < 0 ? 999u : (uint32_t)g_prio_during));
	sdk_puts("\n");
	sdk_puts(g_fail == 0 ? "ipc_pi: PASS\n" : "ipc_pi: FAIL\n");
	ulmk_thread_exit();
}
