/* SPDX-License-Identifier: MIT */
/*
 * preempt_integ — priority preemption integration test
 *
 * Low-priority workers spin-yield without blocking.  A higher-priority
 * waiter blocks on a notification until the supervisor signals it; the
 * kernel must preempt the workers and run the waiter before returning
 * to userspace.
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_printk.h>

static volatile int g_preempt_seen;
static volatile int g_stop;

static ulmk_notif_t g_wake_notif;
static ulmk_notif_t g_done_notif;

#define BIT_WAKE	(1u << 0)
#define BIT_DONE	(1u << 1)

#define PRIO_WORKER	20u
#define PRIO_WAITER	5u
#define PRIO_SUP	15u

static void worker(void *arg)
{
	(void)arg;

	while (!g_stop)
		ulmk_thread_yield();
	ulmk_thread_exit();
}

static void high_prio_waiter(void *arg)
{
	uint32_t bits;

	(void)arg;

	ulmk_notif_wait(g_wake_notif, BIT_WAKE, &bits);
	g_preempt_seen = 1;
	ulmk_notif_signal(g_done_notif, BIT_DONE);
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	ulmk_thread_attr_t attr = {0};
	uint32_t           bits;

	(void)arg;

	ulmk_printk("preempt_integ: start\n");

	g_wake_notif = ulmk_notif_create();
	g_done_notif = ulmk_notif_create();

	attr.stack_size = 1024u;
	attr.privilege  = ULMK_PRIV_USER;
	attr.priority   = PRIO_WORKER;

	attr.name  = "worker_a";
	attr.entry = worker;
	ulmk_thread_create(&attr);

	attr.name = "worker_b";
	ulmk_thread_create(&attr);

	attr.name     = "waiter";
	attr.entry    = high_prio_waiter;
	attr.priority = PRIO_WAITER;
	ulmk_thread_create(&attr);

	ulmk_printk("preempt_integ: signalling waiter\n");
	ulmk_notif_signal(g_wake_notif, BIT_WAKE);

	bits = 0u;
	ulmk_notif_wait(g_done_notif, BIT_DONE, &bits);

	g_stop = 1;

	if (g_preempt_seen)
		ulmk_printk("preempt_integ: PASS\n");
	else
		ulmk_printk("preempt_integ: FAIL seen=%d\n", g_preempt_seen);

	ulmk_sim_exit(g_preempt_seen ? 0 : 1);
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t         tid;

	(void)info;

	attr.name       = "supervisor";
	attr.entry      = supervisor;
	attr.priority   = PRIO_SUP;
	attr.stack_size = 2048u;
	attr.privilege  = ULMK_PRIV_DRIVER;

	tid = ulmk_thread_create(&attr);
	ulmk_cap_grant(tid, ULMK_CAP_SPAWN);
	ulmk_thread_exit();
}
