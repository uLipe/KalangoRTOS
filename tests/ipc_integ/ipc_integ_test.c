/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * IPC integration test — QEMU TriCore TC27x
 *
 * Exercises three scenarios in a single test binary:
 *
 *  1. Ping-pong: one client sends a message to a server, verifies that the
 *     reply payload round-trips correctly.
 *
 *  2. Multi-client: three clients each call the same server sequentially.
 *     The server uses ep_reply_recv for the fast path.  Each client checks
 *     its individual reply label.
 *
 *  3. Notification: a producer thread signals a notification bit; a consumer
 *     thread blocks on notif_wait and verifies the delivered bits.
 *
 *  4. recv_or_notif: server blocks on IPC-or-notif; a client sends IPC first
 *     (IPC path), then a signaller sends a notif (notif path).
 *
 * Boot sequence:
 *   ul_root_thread (prio 20)
 *     └─ spawns supervisor (prio 10) then exits
 *   supervisor
 *     ├─ scenario 1: spawns ping-pong server + client
 *     ├─ waits for sentinels, then scenario 2: spawns multi-client server + 3 clients
 *     ├─ waits, then scenario 3: spawns notif producer + consumer
 *     └─ waits, then scenario 4: spawns recv_or_notif server + ipc client + notif signaller
 *
 * All threads call ul_thread_exit() when done.  The supervisor ul_msleep()s
 * between spawns to avoid the watchdog timer firing.
 */

#include <ul/microkernel.h>
#include "../test_support.h"
#include <kernel/include/ul_printk.h>


/* =========================================================================
 * Scenario 1 — Ping-pong
 * ========================================================================= */

#define LABEL_PING   0xA1
#define LABEL_PONG   0xA2

static ul_ep_t g_pp_ep;

static void pp_server_entry(void *arg)
{
	ul_msg_t msg;
	ul_tid_t sender;

	(void)arg;
	ul_ep_recv(g_pp_ep, &msg, &sender);

	if (msg.label == LABEL_PING && msg.words[0] == 0xDEAD) {
		msg.label    = LABEL_PONG;
		msg.words[0] = 0xBEEF;
		ul_ep_reply(sender, &msg);
		ul_printk("ipc_integ: pp server replied\n");
	} else {
		ul_printk("ipc_integ: pp server BAD MSG\n");
	}
	ul_thread_exit();
}

static void pp_client_entry(void *arg)
{
	ul_msg_t msg = { .label = LABEL_PING, .words = {0xDEAD} };

	(void)arg;
	ul_ep_call(g_pp_ep, &msg);

	if (msg.label == LABEL_PONG && msg.words[0] == 0xBEEF)
		ul_printk("ipc_integ: ping-pong PASS\n");
	else
		ul_printk("ipc_integ: ping-pong FAIL\n");

	ul_thread_exit();
}

/* =========================================================================
 * Scenario 2 — Multi-client (3 clients, 1 server with reply_recv)
 * ========================================================================= */

#define LABEL_REQ    0xB0
#define LABEL_ACK    0xB1

static ul_ep_t g_mc_ep;

static void mc_server_entry(void *arg)
{
	ul_msg_t msg;
	ul_tid_t sender;
	int      i;

	(void)arg;
	ul_printk("ipc_integ: mc_srv starting\n");
	ul_ep_recv(g_mc_ep, &msg, &sender);
	ul_printk("ipc_integ: mc_srv got msg label=0x%x w0=0x%x from tid=%d\n",
		  (unsigned)msg.label, (unsigned)msg.words[0], (int)sender);
	for (i = 0; i < 2; i++) {
		ul_msg_t reply = { .label = LABEL_ACK,
				   .words = {msg.words[0] + 1} };

		ul_ep_reply_recv(g_mc_ep, sender, &reply, &msg, &sender);
	}
	/* Reply to last client. */
	{
		ul_msg_t reply = { .label = LABEL_ACK,
				   .words = {msg.words[0] + 1} };

		ul_ep_reply(sender, &reply);
	}
	ul_printk("ipc_integ: mc server done\n");
	ul_thread_exit();
}

static void mc_client_a_entry(void *arg)
{
	ul_msg_t msg = { .label = LABEL_REQ, .words = {10} };

	(void)arg;
	ul_printk("ipc_integ: mc_cliA starting g_mc_ep=%d\n", (int)g_mc_ep);
	ul_ep_call(g_mc_ep, &msg);

	if (msg.label == LABEL_ACK && msg.words[0] == 11)
		ul_printk("ipc_integ: mc client A PASS\n");
	else
		ul_printk("ipc_integ: mc client A FAIL\n");
	ul_thread_exit();
}

static void mc_client_b_entry(void *arg)
{
	ul_msg_t msg = { .label = LABEL_REQ, .words = {20} };

	(void)arg;
	ul_ep_call(g_mc_ep, &msg);

	if (msg.label == LABEL_ACK && msg.words[0] == 21)
		ul_printk("ipc_integ: mc client B PASS\n");
	else
		ul_printk("ipc_integ: mc client B FAIL\n");
	ul_thread_exit();
}

static void mc_client_c_entry(void *arg)
{
	ul_msg_t msg = { .label = LABEL_REQ, .words = {30} };

	(void)arg;
	ul_ep_call(g_mc_ep, &msg);

	if (msg.label == LABEL_ACK && msg.words[0] == 31)
		ul_printk("ipc_integ: mc client C PASS\n");
	else
		ul_printk("ipc_integ: mc client C FAIL\n");
	ul_thread_exit();
}

/* =========================================================================
 * Scenario 3 — Notification
 * ========================================================================= */

#define NOTIF_BIT_A  (1u << 0)
#define NOTIF_BIT_B  (1u << 1)

static ul_notif_t g_notif;

static void notif_producer_entry(void *arg)
{
	(void)arg;
	ul_msleep(1);   /* ensure consumer is blocked first */
	ul_notif_signal(g_notif, NOTIF_BIT_A | NOTIF_BIT_B);
	ul_printk("ipc_integ: notif producer signalled\n");
	ul_thread_exit();
}

static void notif_consumer_entry(void *arg)
{
	uint32_t bits = 0;

	(void)arg;
	ul_notif_wait(g_notif, NOTIF_BIT_A | NOTIF_BIT_B, &bits);

	if (bits == (NOTIF_BIT_A | NOTIF_BIT_B))
		ul_printk("ipc_integ: notif consumer PASS\n");
	else
		ul_printk("ipc_integ: notif consumer FAIL\n");
	ul_thread_exit();
}

/* =========================================================================
 * Scenario 4 — recv_or_notif
 * ========================================================================= */

#define LABEL_RN_IPC  0xC1
#define NOTIF_BIT_RN  (1u << 2)

static ul_ep_t    g_rn_ep;
static ul_notif_t g_rn_notif;

static void rn_server_entry(void *arg)
{
	ul_msg_t msg;
	ul_tid_t sender;
	uint32_t notif_bits;
	int      ret;

	(void)arg;

	/* Round 1: expect IPC */
	ret = ul_ep_recv_or_notif(g_rn_ep, g_rn_notif, NOTIF_BIT_RN,
				  &msg, &sender, &notif_bits);
	if (ret == 0 && msg.label == LABEL_RN_IPC) {
		ul_msg_t reply = { .label = 0xC2 };

		ul_ep_reply(sender, &reply);
		ul_printk("ipc_integ: rn server IPC path PASS\n");
	} else {
		ul_printk("ipc_integ: rn server IPC path FAIL\n");
	}

	/* Round 2: expect notif */
	ret = ul_ep_recv_or_notif(g_rn_ep, g_rn_notif, NOTIF_BIT_RN,
				  &msg, &sender, &notif_bits);
	if (ret == 1 && (notif_bits & NOTIF_BIT_RN))
		ul_printk("ipc_integ: rn server notif path PASS\n");
	else
		ul_printk("ipc_integ: rn server notif path FAIL\n");

	ul_thread_exit();
}

static void rn_ipc_client_entry(void *arg)
{
	ul_msg_t msg = { .label = LABEL_RN_IPC };

	(void)arg;
	ul_msleep(1);
	ul_ep_call(g_rn_ep, &msg);
	ul_printk("ipc_integ: rn ipc client done\n");
	ul_thread_exit();
}

static void rn_notif_signaller_entry(void *arg)
{
	(void)arg;
	ul_msleep(2);
	ul_notif_signal(g_rn_notif, NOTIF_BIT_RN);
	ul_printk("ipc_integ: rn notif signaller done\n");
	ul_thread_exit();
}

/* =========================================================================
 * Supervisor
 * ========================================================================= */

static void supervisor_entry(void *arg)
{
	ul_thread_attr_t attr;

	(void)arg;
	ul_printk("ipc_integ: start\n");

	ul_msleep(1);
	ul_printk("ipc_integ: timer works after root_thread\n");

	/* ── Scenario 1: ping-pong ─────────────────────────────────────── */
	g_pp_ep = ul_ep_create();

	attr = (ul_thread_attr_t){
		.name = "pp_srv", .entry = pp_server_entry,
		.arg = NULL, .priority = 3,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	attr = (ul_thread_attr_t){
		.name = "pp_cli", .entry = pp_client_entry,
		.arg = NULL, .priority = 4,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	ul_msleep(1);
	ul_printk("ipc_integ: sleep after pp DONE\n");

	/* ── Scenario 2: multi-client ──────────────────────────────────── */
	g_mc_ep = ul_ep_create();

	attr = (ul_thread_attr_t){
		.name = "mc_srv", .entry = mc_server_entry,
		.arg = NULL, .priority = 3,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	{
		ul_tid_t t = ul_thread_create(&attr);
		ul_printk("ipc_integ: mc_srv tid=%d\n", (int)t);
	}

	attr = (ul_thread_attr_t){
		.name = "mc_cliA", .entry = mc_client_a_entry,
		.arg = NULL, .priority = 4,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	{
		ul_tid_t t = ul_thread_create(&attr);
		ul_printk("ipc_integ: mc_cliA tid=%d\n", (int)t);
	}

	attr = (ul_thread_attr_t){
		.name = "mc_cliB", .entry = mc_client_b_entry,
		.arg = NULL, .priority = 5,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	{
		ul_tid_t t = ul_thread_create(&attr);
		ul_printk("ipc_integ: mc_cliB tid=%d\n", (int)t);
	}

	attr = (ul_thread_attr_t){
		.name = "mc_cliC", .entry = mc_client_c_entry,
		.arg = NULL, .priority = 6,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	{
		ul_tid_t t = ul_thread_create(&attr);
		ul_printk("ipc_integ: mc_cliC tid=%d\n", (int)t);
	}

	ul_printk("ipc_integ: spawned all mc threads, sleeping 2ms\n");
	ul_msleep(2);
	ul_printk("ipc_integ: after mc sleep\n");

	/* ── Scenario 3: notification ──────────────────────────────────── */
	g_notif = ul_notif_create();

	attr = (ul_thread_attr_t){
		.name = "n_prod", .entry = notif_producer_entry,
		.arg = NULL, .priority = 4,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	attr = (ul_thread_attr_t){
		.name = "n_cons", .entry = notif_consumer_entry,
		.arg = NULL, .priority = 5,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	ul_msleep(2);

	/* ── Scenario 4: recv_or_notif ─────────────────────────────────── */
	g_rn_ep    = ul_ep_create();
	g_rn_notif = ul_notif_create();

	attr = (ul_thread_attr_t){
		.name = "rn_srv", .entry = rn_server_entry,
		.arg = NULL, .priority = 3,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	attr = (ul_thread_attr_t){
		.name = "rn_icli", .entry = rn_ipc_client_entry,
		.arg = NULL, .priority = 4,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	attr = (ul_thread_attr_t){
		.name = "rn_nsig", .entry = rn_notif_signaller_entry,
		.arg = NULL, .priority = 4,
		.stack_size = 1024, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	ul_msleep(3);

	ul_printk("ipc_integ: PASS\n");
	ul_sim_exit(0);
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr = {
		.name       = "supervisor",
		.entry      = supervisor_entry,
		.arg        = (void *)info,
		.priority   = 10,
		.stack_size = 2048,
		.privilege  = UL_PRIV_DRIVER,
	};
	ul_tid_t sup_tid = ul_thread_create(&attr);

	/* Grant spawn capability so the supervisor can create scenario threads. */
	ul_cap_grant(sup_tid, UL_CAP_SPAWN);

	ul_thread_exit();
}
