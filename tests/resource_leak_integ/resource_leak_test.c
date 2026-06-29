/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Resource leak regression test — tests/resource_leak_integ/
 *
 * Demonstrates that CSA frames and stack memory are correctly returned
 * to their pools when threads exit or are killed.
 *
 * Without the fix (ul_arch_ctx_free / ul_phys_free never called on
 * thread death), each thread permanently consumes its CSA frames.  The
 * 256-frame pool is exhausted after roughly 50-80 threads, causing
 * ul_arch_cpu_halt() to be called from csa_alloc(), freezing QEMU.
 * The test would then time out and be reported as FAIL.
 *
 * With the fix, CSA frames and stack memory are reclaimed by the deferred
 * reaper in ul_sched_schedule().  All WAVES * THREADS_PER_WAVE threads
 * complete successfully and the supervisor prints "resource_leak: PASS".
 *
 * Numbers:
 *   CSA pool      256 frames × 64 B = 16 KiB
 *   Per-thread    ≥3 frames consumed at exit (initial chain + syscall + ctx_switch)
 *   Without fix   exhaustion at ≈80 threads → hangs before wave 11
 *   With fix      160 threads across 20 waves → PASS
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>

extern void qemu_virt_exit(uint32_t code);

#define WAVES			20
#define THREADS_PER_WAVE	8

/*
 * g_done_count — incremented by each worker just before ul_thread_exit().
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
	ul_thread_yield();
	g_done_count++;
	g_total_threads++;
	ul_thread_exit();
}

/* =========================================================================
 * Supervisor
 * ========================================================================= */

static void supervisor_entry(void *arg)
{
	ul_thread_attr_t	attr;
	int			wave;
	int			spawned;
	int			j;
	ul_tid_t		tid;

	(void)arg;

	ul_printk("resource_leak: start (waves=%d threads_per_wave=%d)\n",
		  WAVES, THREADS_PER_WAVE);

	attr.name      = "w";
	attr.entry     = worker_entry;
	attr.arg       = NULL;
	attr.priority  = 1;		/* higher than supervisor (prio=100) */
	attr.stack_size = 512;
	attr.privilege  = UL_PRIV_DRIVER;

	for (wave = 0; wave < WAVES; wave++) {
		g_done_count = 0;
		spawned      = 0;

		for (j = 0; j < THREADS_PER_WAVE; j++) {
			tid = ul_thread_create(&attr);
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
			ul_thread_yield();

		ul_printk("resource_leak: wave %d/%d done\n",
			  wave + 1, WAVES);
	}

	ul_printk("resource_leak: total_threads=%d\n", g_total_threads);

	if (g_total_threads == WAVES * THREADS_PER_WAVE) {
		ul_printk("resource_leak: PASS\n");
	} else {
		ul_printk("resource_leak: FAIL (expected %d got %d)\n",
			  WAVES * THREADS_PER_WAVE, g_total_threads);
	}

	qemu_virt_exit(0);
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;
	ul_tid_t	 sup_tid;

	(void)info;

	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 100;
	attr.stack_size = 2048;
	attr.privilege  = UL_PRIV_DRIVER;

	sup_tid = ul_thread_create(&attr);
	/*
	 * The supervisor must spawn workers itself, so it needs UL_CAP_SPAWN.
	 * Root thread holds UL_CAP_ALL and can grant to any thread.
	 */
	ul_cap_grant(sup_tid, UL_CAP_SPAWN);
	ul_thread_exit();
}
