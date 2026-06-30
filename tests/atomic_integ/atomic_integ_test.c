/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Atomic operations + context-switch atomicity integration test
 * tests/atomic_integ/atomic_integ_test.c
 *
 * Phase 1 — Atomic counter correctness:
 *   Two worker threads at the same priority run concurrently under
 *   preemptive round-robin scheduling.  Each calls ul_arch_atomic_add()
 *   N times on a shared counter.  The expected final value is 2*N.
 *   Without a working atomic_add the increments race and are lost.
 *
 * Phase 2 — Context-switch atomicity stress:
 *   Eight threads at the same priority each perform M cooperative
 *   ul_thread_yield() calls in a tight loop while the timer fires every
 *   quantum.  An interrupt landing inside ul_arch_ctx_switch() between
 *   "mtcr PCXI = to->pcxi" and "rslcx" would prepend ISR CSA frames to
 *   the destination thread's chain, corrupting it.  The test succeeds
 *   if all threads complete without a context-management trap (class 3).
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ul/microkernel.h>
#include <ul_arch.h>
#include <kernel/include/ul_printk.h>


/* =========================================================================
 * Phase 1 — Atomic counter
 * ========================================================================= */

#define ATOMIC_ITERS	50000u

static volatile uint32_t	g_counter;
static volatile int		g_phase1_done;

static void atomic_worker(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0; i < ATOMIC_ITERS; i++)
		ul_arch_atomic_add(&g_counter, 1u);

	ul_arch_atomic_add((volatile uint32_t *)&g_phase1_done, 1u);
	ul_thread_exit();
}

/* =========================================================================
 * Phase 2 — Context-switch stress
 * ========================================================================= */

/*
 * Workers run at a lower priority (higher number) than the supervisor so
 * the supervisor can wake from ul_usleep and preempt them.  Giving workers
 * higher urgency (prio < supervisor prio) would prevent the supervisor from
 * ever running until every worker exited.
 */
#define CTX_YIELD_ITERS		100u
#define CTX_WORKERS		8
#define CTX_WORKER_PRIO		15

static volatile int	g_ctx_done;

static void ctx_worker(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0; i < CTX_YIELD_ITERS; i++)
		ul_thread_yield();

	ul_arch_atomic_add((volatile uint32_t *)&g_ctx_done, 1u);
	ul_thread_exit();
}

/* =========================================================================
 * Supervisor
 * ========================================================================= */

static void supervisor_entry(void *arg)
{
	ul_thread_attr_t attr;
	int		 i;

	(void)arg;
	ul_printk("atomic_integ: start\n");

	/* --- Phase 1: atomic counter ------------------------------------ */
	ul_printk("atomic_integ: phase1 start (2 workers x %u iters)\n",
		  ATOMIC_ITERS);

	g_counter    = 0u;
	g_phase1_done = 0;

	attr.name      = "aw";
	attr.entry     = atomic_worker;
	attr.arg       = NULL;
	attr.priority  = 5;
	attr.stack_size = 512;
	attr.privilege  = UL_PRIV_DRIVER;

	ul_thread_create(&attr);
	ul_thread_create(&attr);

	/* Supervisor (prio 10) sleeps while workers (prio 5) run. */
	ul_usleep(500000u);	/* 500 ms — generous budget for QEMU */

	if (g_phase1_done != 2) {
		ul_printk("atomic_integ: phase1 workers did not finish "
			  "(done=%d)\n", g_phase1_done);
		ul_printk("atomic_integ: FAIL\n");
		ul_sim_exit(1);
	}

	if (g_counter != 2u * ATOMIC_ITERS) {
		ul_printk("atomic_integ: phase1 counter mismatch "
			  "(got %lu expected %lu)\n",
			  (unsigned long)g_counter,
			  (unsigned long)(2u * ATOMIC_ITERS));
		ul_printk("atomic_integ: FAIL\n");
		ul_sim_exit(1);
	}
	ul_printk("atomic_integ: phase1 PASS (counter=%lu)\n",
		  (unsigned long)g_counter);

	/* --- Phase 2: ctx-switch stress --------------------------------- */
	ul_printk("atomic_integ: phase2 start (%d workers x %u yields)\n",
		  CTX_WORKERS, CTX_YIELD_ITERS);

	g_ctx_done = 0;

	attr.name      = "cw";
	attr.entry     = ctx_worker;
	attr.arg       = NULL;
	attr.priority  = CTX_WORKER_PRIO;
	attr.stack_size = 512;
	attr.privilege  = UL_PRIV_DRIVER;

	for (i = 0; i < CTX_WORKERS; i++)
		ul_thread_create(&attr);

	/*
	 * Workers run at lower priority; supervisor can preempt after waking.
	 * 20 s gives ample room for QEMU to complete 8 x 100 cooperative
	 * yields (800 context switches) before the check fires.
	 */
	ul_usleep(60000000u);	/* 60 s */

	if (g_ctx_done != CTX_WORKERS) {
		ul_printk("atomic_integ: phase2 not all workers done "
			  "(done=%d/%d)\n", g_ctx_done, CTX_WORKERS);
		ul_printk("atomic_integ: FAIL\n");
		ul_sim_exit(1);
	}
	ul_printk("atomic_integ: phase2 PASS\n");

	ul_printk("atomic_integ: PASS\n");
	ul_sim_exit(0);
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;
	ul_tid_t	 tid;

	(void)info;

	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 10;
	attr.stack_size = 2048;
	attr.privilege  = UL_PRIV_DRIVER;

	tid = ul_thread_create(&attr);
	ul_cap_grant(tid, UL_CAP_SPAWN);
	ul_thread_exit();
}
