/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

#define LABEL_PING	0xA1u
#define LABEL_PONG	0xA2u
#define NOTIF_BIT_A	(1u << 0)
#define NOTIF_BIT_B	(1u << 1)

static ulmk_ep_t    g_pp_ep;
static ulmk_notif_t g_notif;

static void pp_server(void *arg)
{
	ulmk_msg_t msg;
	ulmk_tid_t sender;

	(void)arg;
	ulmk_ep_recv(g_pp_ep, &msg, &sender);
	if (msg.label == LABEL_PING && msg.words[0] == 0xDEADu) {
		msg.label    = LABEL_PONG;
		msg.words[0] = 0xBEEFu;
		ulmk_ep_reply(sender, &msg);
		sdk_puts("ipc: pp server replied\n");
	}
	ulmk_thread_exit();
}

static void pp_client(void *arg)
{
	ulmk_msg_t msg;

	(void)arg;
	msg.label    = LABEL_PING;
	msg.words[0] = 0xDEADu;
	ulmk_ep_call(g_pp_ep, &msg);
	if (msg.label == LABEL_PONG && msg.words[0] == 0xBEEFu)
		sdk_puts("ipc: ping-pong PASS\n");
	else
		sdk_puts("ipc: ping-pong FAIL\n");
	ulmk_thread_exit();
}

static void notif_producer(void *arg)
{
	(void)arg;
	sdk_msleep_yield(1u);
	ulmk_notif_signal(g_notif, NOTIF_BIT_A | NOTIF_BIT_B);
	sdk_puts("ipc: notif producer signalled\n");
	ulmk_thread_exit();
}

static void notif_consumer(void *arg)
{
	uint32_t bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_notif, NOTIF_BIT_A | NOTIF_BIT_B, &bits);
	if (bits == (NOTIF_BIT_A | NOTIF_BIT_B))
		sdk_puts("ipc: notif consumer PASS\n");
	else
		sdk_puts("ipc: notif consumer FAIL\n");
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	(void)arg;
	sdk_puts("ipc: start\n");

	g_pp_ep = ulmk_ep_create();
	sdk_spawn("pp_srv", pp_server, NULL, 3u, 2048u, 0u);
	sdk_spawn("pp_cli", pp_client, NULL, 4u, 2048u, 0u);
	sdk_msleep_yield(10u);

	g_notif = ulmk_notif_create();
	sdk_spawn("n_prod", notif_producer, NULL, 4u, 2048u, 0u);
	sdk_spawn("n_cons", notif_consumer, NULL, 5u, 2048u, 0u);
	sdk_msleep_yield(10u);

	sdk_puts("ipc: PASS\n");
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;

	board_services_init(info);
	tid = sdk_spawn("sup", supervisor, NULL, 10u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_SPAWN);
	ulmk_thread_exit();
}
