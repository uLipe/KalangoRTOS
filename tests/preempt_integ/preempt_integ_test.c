/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * preempt_integ — preemptive round-robin integration test
 *
 * Two worker threads at equal priority run tight busy-loops.  Without
 * preemption the first worker monopolises the CPU and the second never
 * accumulates any count.  With round-robin preemption both counters grow
 * because the scheduler rotates them at every quantum boundary.
 *
 * A supervisor thread sleeps for 200 ms (20 quanta at 10 ms each), then
 * wakes up (its higher priority preempts the workers) and checks that both
 * counters are non-zero.
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>


/* -------------------------------------------------------------------------
 * Shared state (volatile: modified by worker threads, read by supervisor)
 * ---------------------------------------------------------------------- */
static volatile uint32_t g_count_a;
static volatile uint32_t g_count_b;
static volatile int      g_stop;

/* -------------------------------------------------------------------------
 * Worker threads — busy-loop until supervisor sets g_stop
 * ---------------------------------------------------------------------- */
static void worker_a(void *arg)
{
	(void)arg;
	while (!g_stop) {
		g_count_a++;
	}
	ul_thread_exit();
}

static void worker_b(void *arg)
{
	(void)arg;
	while (!g_stop) {
		g_count_b++;
	}
	ul_thread_exit();
}

/* -------------------------------------------------------------------------
 * Supervisor — sleeps, then validates round-robin progress
 * ---------------------------------------------------------------------- */
static void supervisor(void *arg)
{
	ul_thread_attr_t attr;
	ul_tid_t         tid_a;
	ul_tid_t         tid_b;
	uint32_t         a;
	uint32_t         b;

	(void)arg;

	ul_printk("preempt_integ: start\n");

	/* Spawn both workers at equal, lower priority than supervisor (prio 0).
	 * Workers run while supervisor is blocked in sleep. */
	attr.name       = "worker_a";
	attr.entry      = worker_a;
	attr.arg        = NULL;
	attr.priority   = 5;
	attr.stack_size = 1024;
	attr.privilege  = UL_PRIV_USER;
	tid_a = ul_thread_create(&attr);
	ul_printk("preempt_integ: worker_a tid=%d\n", (int)tid_a);

	attr.name       = "worker_b";
	attr.entry      = worker_b;
	attr.priority   = 5;   /* same priority — round-robin expected */
	tid_b = ul_thread_create(&attr);
	ul_printk("preempt_integ: worker_b tid=%d\n", (int)tid_b);

	/*
	 * Block for 200 ms so workers get the CPU.  A yield-loop cannot work
	 * here: supervisor (prio 0) would immediately reclaim the CPU on every
	 * yield, starving the lower-priority workers.  The timer removes the
	 * supervisor from the run queue for the full duration.
	 */
	ul_printk("preempt_integ: sleeping 200ms\n");
	ul_timer_set_deadline(200000ULL);
	ul_timer_wait();
	ul_printk("preempt_integ: awoke\n");

	/* Stop workers */
	g_stop = 1;

	a = g_count_a;
	b = g_count_b;

	ul_printk("preempt_integ: a=%u b=%u\n", (unsigned)a, (unsigned)b);

	if (a > 0 && b > 0) {
		ul_printk("preempt_integ: PASS\n");
	} else {
		ul_printk("preempt_integ: FAIL (a=%u b=%u)\n",
			  (unsigned)a, (unsigned)b);
	}

	ul_sim_exit(0);
}

/* -------------------------------------------------------------------------
 * Root thread
 * ---------------------------------------------------------------------- */
void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;

	(void)info;

	attr.name       = "supervisor";
	attr.entry      = supervisor;
	attr.arg        = NULL;
	attr.priority   = 0;   /* highest: wakes from sleep and preempts workers */
	attr.stack_size = 2048;
	attr.privilege  = UL_PRIV_DRIVER;

	ul_tid_t sup_tid = ul_thread_create(&attr);
	ul_cap_grant(sup_tid, UL_CAP_SPAWN);
	ul_cap_grant(sup_tid, UL_CAP_TIMER);

	ul_thread_exit();
}
