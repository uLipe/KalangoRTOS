/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Atomic operations + context-switch atomicity integration test
 * tests/atomic_integ/atomic_integ_test.c
 *
 * Phase 1 (atomic correctness) runs in ulmk_kernel_pre_root_hook() before
 * the scheduler starts — see atomic_hook.c.
 *
 * Phase 2 — Context-switch atomicity stress:
 *   Eight threads at the same priority each perform M cooperative
 *   ulmk_thread_yield() calls in a tight loop while the timer fires every
 *   quantum.  An interrupt landing inside ulmk_arch_ctx_switch() between
 *   "mtcr PCXI = to->pcxi" and "rslcx" would prepend ISR CSA frames to
 *   the destination thread's chain, corrupting it.  The test succeeds
 *   if all threads complete without a context-management trap (class 3).
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>

#define CTX_YIELD_ITERS		100u
#define CTX_WORKERS		8
#define CTX_WORKER_PRIO		15

static volatile int	g_ctx_done;

static void ctx_worker(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0u; i < CTX_YIELD_ITERS; i++)
		ulmk_thread_yield();

	g_ctx_done++;
	ulmk_thread_exit();
}

static void supervisor_entry(void *arg)
{
	ulmk_thread_attr_t attr = {0};
	int		 i;

	(void)arg;

	ulmk_printk("atomic_integ: phase2 start (%d workers x %u yields)\n",
		  CTX_WORKERS, CTX_YIELD_ITERS);

	g_ctx_done = 0;

	attr.name       = "cw";
	attr.entry      = ctx_worker;
	attr.arg        = NULL;
	attr.priority   = CTX_WORKER_PRIO;
	attr.stack_size = 512;
	attr.privilege  = ULMK_PRIV_DRIVER;

	for (i = 0; i < CTX_WORKERS; i++)
		ulmk_thread_create(&attr);

	/*
	 * Poll with short blocking delays until all workers finish.
	 * The timer removes the supervisor from the run queue so that
	 * lower-priority workers get the CPU between checks.
	 */
	{
		uint32_t waited = 0u;

		while (g_ctx_done != CTX_WORKERS && waited < 40000u) {
			ulmk_msleep(1u);
			waited++;
		}
	}

	if (g_ctx_done != CTX_WORKERS) {
		ulmk_printk("atomic_integ: phase2 not all workers done "
			  "(done=%d/%d)\n", g_ctx_done, CTX_WORKERS);
		ulmk_printk("atomic_integ: FAIL\n");
		ulmk_sim_exit(1);
	}

	ulmk_printk("atomic_integ: phase2 PASS\n");
	ulmk_printk("atomic_integ: PASS\n");
	ulmk_sim_exit(0);
}

extern void atomic_integ_phase1(void);

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t	 tid;

	(void)info;

	atomic_integ_phase1();

	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 25u;
	attr.stack_size = 2048;
	attr.privilege  = ULMK_PRIV_DRIVER;

	tid = ulmk_thread_create(&attr);
	ulmk_cap_grant(tid, ULMK_CAP_SPAWN);
	ulmk_thread_exit();
}
