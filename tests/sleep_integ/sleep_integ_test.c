/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Sleep integration test — tests/sleep_integ/sleep_integ_test.c
 *
 * Validates the timer server pattern:
 *   - A dedicated timer server thread holds ULMK_CAP_TIMER and uses
 *     ulmk_timer_set_deadline() + ulmk_timer_wait() to sleep.
 *   - User threads request sleep by sending an IPC message containing
 *     the desired duration in µs.  They block on ep_call() until the
 *     timer server replies.
 *
 * Scenario: three threads (A prio=1, B prio=2, C prio=3) each call the
 * timer server 10 times asking for a 10 ms sleep.  The timer server handles
 * requests serially: one caller at a time.
 *
 * Expected outcome: all three threads complete and the last one prints the
 * PASS sentinel before calling ulmk_sim_exit(0).
 *
 * Sentinels verified by the Makefile:
 *   sleep_integ: start
 *   sleep_integ: thread A alive (sleep 10)
 *   sleep_integ: thread A done
 *   sleep_integ: thread B done
 *   sleep_integ: thread C done
 *   sleep_integ: PASS
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_printk.h>

#define SLEEP_COUNT	10u
#define SLEEP_US	10000u		/* 10 ms per sleep in µs */

/* =========================================================================
 * Timer server
 * ========================================================================= */

static ulmk_ep_t   g_timer_ep;
static uint32_t  g_done_count;

static void timer_server_entry(void *arg)
{
	ulmk_msg_t  req;
	ulmk_msg_t  reply;
	ulmk_tid_t  sender;

	(void)arg;

	reply.label    = 0u;
	reply.words[0] = 0u;

	while (1) {
		ulmk_ep_recv(g_timer_ep, &req, &sender);
		ulmk_timer_set_deadline((uint64_t)req.words[0]);
		ulmk_timer_wait();
		ulmk_ep_reply(sender, &reply);
	}
}

/* =========================================================================
 * Helper: sleep via timer server
 * ========================================================================= */

static void timer_sleep_us(uint32_t us)
{
	ulmk_msg_t msg;

	msg.label    = 0u;
	msg.words[0] = us;
	ulmk_ep_call(g_timer_ep, &msg);
}

/* =========================================================================
 * Thread entries
 * ========================================================================= */

static void thread_a_entry(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0; i < SLEEP_COUNT; i++) {
		timer_sleep_us(SLEEP_US);
		if (i == 9u)
			ulmk_printk("sleep_integ: thread A alive (sleep 10)\n");
	}
	ulmk_printk("sleep_integ: thread A done\n");

	g_done_count++;
	if (g_done_count == 3u) {
		ulmk_printk("sleep_integ: PASS\n");
		ulmk_sim_exit(0);
	}
	ulmk_thread_exit();
}

static void thread_b_entry(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0; i < SLEEP_COUNT; i++)
		timer_sleep_us(SLEEP_US);

	ulmk_printk("sleep_integ: thread B done\n");

	g_done_count++;
	if (g_done_count == 3u) {
		ulmk_printk("sleep_integ: PASS\n");
		ulmk_sim_exit(0);
	}
	ulmk_thread_exit();
}

static void thread_c_entry(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0; i < SLEEP_COUNT; i++)
		timer_sleep_us(SLEEP_US);

	ulmk_printk("sleep_integ: thread C done\n");

	g_done_count++;
	if (g_done_count == 3u) {
		ulmk_printk("sleep_integ: PASS\n");
		ulmk_sim_exit(0);
	}
	ulmk_thread_exit();
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr;
	ulmk_tid_t         tsrv_tid;

	(void)info;

	ulmk_printk("sleep_integ: start\n");

	g_timer_ep   = ulmk_ep_create();
	g_done_count = 0u;

	attr.arg       = NULL;
	attr.privilege = ULMK_PRIV_DRIVER;

	/* Timer server — highest priority so it serves sleepers promptly. */
	attr.name       = "tsrv";
	attr.entry      = timer_server_entry;
	attr.priority   = 0;
	attr.stack_size = 2048;
	tsrv_tid = ulmk_thread_create(&attr);
	ulmk_cap_grant(tsrv_tid, ULMK_CAP_TIMER);
	ulmk_ep_grant(g_timer_ep, tsrv_tid);

	/* Worker threads — share the timer server endpoint. */
	attr.name       = "a";
	attr.entry      = thread_a_entry;
	attr.priority   = 1;
	attr.stack_size = 2048;
	ulmk_thread_create(&attr);

	attr.name       = "b";
	attr.entry      = thread_b_entry;
	attr.priority   = 2;
	ulmk_thread_create(&attr);

	attr.name       = "c";
	attr.entry      = thread_c_entry;
	attr.priority   = 3;
	ulmk_thread_create(&attr);

	ulmk_thread_exit();
}
