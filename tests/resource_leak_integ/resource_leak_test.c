/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Resource leak regression test — tests/resource_leak_integ/
 *
 * Demonstrates that CSA frames and stack memory are correctly returned
 * to their pools when threads exit or are killed.
 *
 * Without the fix (ulmk_arch_ctx_free / ulmk_phys_free never called on
 * thread death), each thread permanently consumes its CSA frames.  The
 * 256-frame pool is exhausted after roughly 50-80 threads, causing
 * ulmk_arch_cpu_halt() to be called from csa_alloc(), freezing QEMU.
 * The test would then time out and be reported as FAIL.
 *
 * With the fix, CSA frames and stack memory are reclaimed by the deferred
 * reaper in ulmk_sched_schedule().  All WAVES * THREADS_PER_WAVE threads
 * complete successfully and the supervisor prints "resource_leak: PASS".
 *
 * Numbers:
 *   CSA pool      256 frames × 64 B = 16 KiB
 *   Per-thread    ≥3 frames consumed at exit (initial chain + syscall + ctx_switch)
 *   Without fix   exhaustion at ≈80 threads → hangs before wave 11
 *   With fix      160 threads across 20 waves → PASS
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_printk.h>


#define WAVES			20
#define THREADS_PER_WAVE	8

/*
 * g_done_count — incremented by each worker just before ulmk_thread_exit().
 * Reset to 0 at the start of each wave.  Single-core cooperative scheduling
 * makes a plain volatile int sufficient.
 */
static volatile int	g_done_count;
static volatile int	g_total_threads;

/* =========================================================================
 * Worker thread
 * ========================================================================= */

static void worker_entry(void *arg)
{
	(void)arg;
	/*
	 * One yield to exercise the context-switch path before exit.
	 * Keeps the test cooperative and representative of real usage.
	 */
	ulmk_thread_yield();
	g_done_count++;
	g_total_threads++;
	ulmk_thread_exit();
}

/* =========================================================================
 * Supervisor
 * ========================================================================= */

static void supervisor_entry(void *arg)
{
	ulmk_thread_attr_t	attr = {0};
	int			wave;
	int			spawned;
	int			j;
	ulmk_tid_t		tid;

	(void)arg;

	ulmk_printk("resource_leak: start (waves=%d threads_per_wave=%d)\n",
		  WAVES, THREADS_PER_WAVE);

	attr.name      = "w";
	attr.entry     = worker_entry;
	attr.arg       = NULL;
	attr.priority  = 1;		/* higher than supervisor (prio=100) */
	attr.stack_size = 512;
	attr.privilege  = ULMK_PRIV_DRIVER;

	for (wave = 0; wave < WAVES; wave++) {
		g_done_count = 0;
		spawned      = 0;

		for (j = 0; j < THREADS_PER_WAVE; j++) {
			tid = ulmk_thread_create(&attr);
			if (tid >= 0)
				spawned++;
		}

		/*
		 * Yield until all workers of this wave have exited.
		 * Because workers have priority 1 and supervisor has priority
		 * 100, the scheduler always picks a ready worker first.
		 * When all workers are dead, the supervisor resumes.
		 */
		while (g_done_count < spawned)
			ulmk_thread_yield();

		ulmk_printk("resource_leak: wave %d/%d done\n",
			  wave + 1, WAVES);
	}

	ulmk_printk("resource_leak: total_threads=%d\n", g_total_threads);

	if (g_total_threads == WAVES * THREADS_PER_WAVE) {
		ulmk_printk("resource_leak: PASS\n");
	} else {
		ulmk_printk("resource_leak: FAIL (expected %d got %d)\n",
			  WAVES * THREADS_PER_WAVE, g_total_threads);
	}

	ulmk_sim_exit(0);
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t	 sup_tid;

	(void)info;

	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 100;
	attr.stack_size = 2048;
	attr.privilege  = ULMK_PRIV_DRIVER;

	sup_tid = ulmk_thread_create(&attr);
	/*
	 * The supervisor must spawn workers itself, so it needs ULMK_CAP_SPAWN.
	 * Root thread holds ULMK_CAP_ALL and can grant to any thread.
	 */
	ulmk_cap_grant(sup_tid, ULMK_CAP_SPAWN);
	ulmk_thread_exit();
}
