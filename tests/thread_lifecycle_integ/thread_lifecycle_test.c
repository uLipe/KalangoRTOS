/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Thread lifecycle integration test — tests/thread_lifecycle_integ/
 *
 * Exercises ulmk_thread_create (ulmk_kern_thread_spawn), ulmk_thread_kill,
 * ulmk_thread_suspend, ulmk_thread_resume, ulmk_thread_priority_set/get,
 * and ulmk_thread_self — all through the syscall gateway.
 *
 * Scenario
 * --------
 *   Root thread (prio=200) spawns four worker threads via ulmk_thread_create:
 *
 *   worker_a (prio=1) — runs 10 iterations: ulmk_thread_yield() each round,
 *     prints progress, exits via ulmk_thread_exit().
 *
 *   worker_b (prio=2) — same as A; spawned by root AFTER A.
 *
 *   worker_c (prio=3) — spawned but immediately suspended by the root
 *     thread; then resumed once A reports it is done.
 *
 *   worker_kill (prio=4) — spawned last; root kills it before it can print.
 *
 * Expected output (sentinels checked by Makefile):
 *   thread_lifecycle: start
 *   thread_lifecycle: A started
 *   thread_lifecycle: A done
 *   thread_lifecycle: B started
 *   thread_lifecycle: B done
 *   thread_lifecycle: C resumed
 *   thread_lifecycle: C done
 *   thread_lifecycle: kill target never ran
 *   thread_lifecycle: PASS
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_printk.h>


/* =========================================================================
 * Shared flags — no atomics needed (single-core cooperative).
 * ========================================================================= */

static volatile int g_a_done;
static volatile int g_b_done;
static volatile int g_c_done;
static volatile int g_kill_ran;

/* =========================================================================
 * Worker entries
 * ========================================================================= */

static void worker_a_entry(void *arg)
{
	int i;

	(void)arg;
	ulmk_printk("thread_lifecycle: A started\n");

	for (i = 0; i < 10; i++)
		ulmk_thread_yield();

	ulmk_printk("thread_lifecycle: A done\n");
	g_a_done = 1;
	ulmk_thread_exit();
}

static void worker_b_entry(void *arg)
{
	int i;

	(void)arg;
	ulmk_printk("thread_lifecycle: B started\n");

	for (i = 0; i < 10; i++)
		ulmk_thread_yield();

	ulmk_printk("thread_lifecycle: B done\n");
	g_b_done = 1;
	ulmk_thread_exit();
}

static void worker_c_entry(void *arg)
{
	int i;

	(void)arg;
	ulmk_printk("thread_lifecycle: C resumed\n");

	for (i = 0; i < 10; i++)
		ulmk_thread_yield();

	ulmk_printk("thread_lifecycle: C done\n");
	g_c_done = 1;
	ulmk_thread_exit();
}

static void worker_kill_entry(void *arg)
{
	(void)arg;
	/* If we reach here, the kill test failed. */
	ulmk_printk("thread_lifecycle: FAIL kill target ran!\n");
	g_kill_ran = 1;
	ulmk_thread_exit();
}

/* =========================================================================
 * Supervisor thread — orchestrates the lifecycle test
 * ========================================================================= */

static void supervisor_entry(void *arg)
{
	ulmk_tid_t	     tid_a, tid_b, tid_c, tid_kill;
	ulmk_thread_attr_t attr;
	int		     prio;

	(void)arg;

	ulmk_printk("thread_lifecycle: start\n");

	/*
	 * Supervisor priority = 10.  Workers have priorities 1..4.
	 * When supervisor yields, the scheduler picks the highest-priority
	 * ready thread (lowest number), so workers run before supervisor.
	 */
	attr.privilege  = ULMK_PRIV_DRIVER;
	attr.arg        = NULL;

	/* Spawn A (prio 1 — runs first). */
	attr.name       = "A";
	attr.entry      = worker_a_entry;
	attr.priority   = 1;
	attr.stack_size = 1024;
	tid_a = ulmk_thread_create(&attr);

	/* Spawn B (prio 2). */
	attr.name       = "B";
	attr.entry      = worker_b_entry;
	attr.priority   = 2;
	tid_b = ulmk_thread_create(&attr);

	/* Spawn C (prio 3) then immediately suspend it. */
	attr.name       = "C";
	attr.entry      = worker_c_entry;
	attr.priority   = 3;
	tid_c = ulmk_thread_create(&attr);
	ulmk_thread_suspend(tid_c);

	/* Spawn kill target (prio 4) then kill it before it can run. */
	attr.name       = "K";
	attr.entry      = worker_kill_entry;
	attr.priority   = 4;
	tid_kill = ulmk_thread_create(&attr);
	ulmk_thread_kill(tid_kill);

	/* Yield until A and B finish.  Supervisor prio=10 < worker prio=1,2
	 * so each yield switches to the next ready worker. */
	while (!g_a_done || !g_b_done)
		ulmk_thread_yield();

	/* C was suspended — resume it now. */
	ulmk_thread_resume(tid_c);

	/* Wait for C to finish. */
	while (!g_c_done)
		ulmk_thread_yield();

	/* Verify kill target never ran. */
	if (!g_kill_ran)
		ulmk_printk("thread_lifecycle: kill target never ran\n");

	/* Basic priority API smoke test. */
	ulmk_thread_priority_set(ulmk_thread_self(), 200);
	prio = ulmk_thread_priority_get(ulmk_thread_self());
	if (prio == 200)
		ulmk_printk("thread_lifecycle: PASS\n");
	else
		ulmk_printk("thread_lifecycle: FAIL prio mismatch\n");

	/* Suppress unused-TID warnings. */
	(void)tid_a;
	(void)tid_b;

	ulmk_sim_exit(0);
}

/* =========================================================================
 * Root thread — hand off to supervisor, then exit
 * ========================================================================= */

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr;
	ulmk_tid_t	 sup_tid;

	(void)info;

	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 10;	/* lower than workers (1..4) so they run first */
	attr.stack_size = 2048;
	attr.privilege  = ULMK_PRIV_DRIVER;

	sup_tid = ulmk_thread_create(&attr);
	ulmk_cap_grant(sup_tid, ULMK_CAP_SPAWN | ULMK_CAP_KILL);
	ulmk_thread_exit();
}
