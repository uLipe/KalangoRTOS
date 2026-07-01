/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Atomic operations + context-switch atomicity integration test
 * tests/atomic_integ/atomic_integ_test.c
 *
 * Phase 1 (atomic correctness) runs in ul_kernel_pre_root_hook() before
 * the scheduler starts — see atomic_hook.c.
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

#define CTX_YIELD_ITERS		100u
#define CTX_WORKERS		8
#define CTX_WORKER_PRIO		15

static volatile int	g_ctx_done;

static void ctx_worker(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0u; i < CTX_YIELD_ITERS; i++)
		ul_thread_yield();

	ul_arch_atomic_add((volatile uint32_t *)&g_ctx_done, 1u);
	ul_thread_exit();
}

static void supervisor_entry(void *arg)
{
	ul_thread_attr_t attr;
	int		 i;

	(void)arg;

	ul_printk("atomic_integ: phase2 start (%d workers x %u yields)\n",
		  CTX_WORKERS, CTX_YIELD_ITERS);

	g_ctx_done = 0;

	attr.name       = "cw";
	attr.entry      = ctx_worker;
	attr.arg        = NULL;
	attr.priority   = CTX_WORKER_PRIO;
	attr.stack_size = 512;
	attr.privilege  = UL_PRIV_DRIVER;

	for (i = 0; i < CTX_WORKERS; i++)
		ul_thread_create(&attr);

	/*
	 * Poll with short sleeps until all workers finish.
	 * Each ul_msleep(1) suspends the supervisor for one tick, giving
	 * lower-priority workers CPU time to run their yields.
	 */
	{
		uint32_t waited = 0u;

		while (g_ctx_done != CTX_WORKERS && waited < 2000u) {
			ul_msleep(1u);
			waited++;
		}
	}

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
