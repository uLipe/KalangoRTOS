/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Scheduler integration test — tests/sched_integ/sched_integ_test.c
 *
 * Scenario: three threads (high/mid/low priority) + root.
 * Root lowers its own priority to 200, spawns the three threads via
 * ulmk_thread_create, then yields.
 *
 * Expected execution order:
 *   high (prio=1):  "high step 1" → yield → "high step 2" → suspend
 *   mid  (prio=2):  "mid step 1"  → yield → "mid step 2"  → suspend
 *   low  (prio=3):  "low step 1"  → suspend
 *   root (prio=200): resumes from yield → prints PASS → exits
 *
 * Sentinels (in order):
 *   sched_integ: high step 1
 *   sched_integ: high step 2
 *   sched_integ: mid step 1
 *   sched_integ: mid step 2
 *   sched_integ: low step 1
 *   sched_integ: PASS
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_printk.h>


/* =========================================================================
 * Thread entry points
 * ========================================================================= */

static void high_thread_entry(void *arg)
{
	(void)arg;
	ulmk_printk("sched_integ: high step 1\n");
	ulmk_thread_yield();
	ulmk_printk("sched_integ: high step 2\n");
	ulmk_thread_suspend(ulmk_thread_self());
	for (;;)
		;
}

static void mid_thread_entry(void *arg)
{
	(void)arg;
	ulmk_printk("sched_integ: mid step 1\n");
	ulmk_thread_yield();
	ulmk_printk("sched_integ: mid step 2\n");
	ulmk_thread_suspend(ulmk_thread_self());
	for (;;)
		;
}

static void low_thread_entry(void *arg)
{
	(void)arg;
	ulmk_printk("sched_integ: low step 1\n");
	ulmk_thread_suspend(ulmk_thread_self());
	for (;;)
		;
}

/* =========================================================================
 * Root thread — provided as ulmk_root_thread() per boot model
 * ========================================================================= */

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};

	(void)info;

	ulmk_printk("sched_integ: start\n");

	/*
	 * Lower root's priority so higher-priority test threads run first.
	 * Root was created with prio=0 by kernel_main.c; set it to 200.
	 */
	ulmk_thread_priority_set(ulmk_thread_self(), 200);

	attr.arg       = NULL;
	attr.privilege = ULMK_PRIV_DRIVER;

	attr.name       = "high";
	attr.entry      = high_thread_entry;
	attr.priority   = 1;
	attr.stack_size = 1024;
	ulmk_thread_create(&attr);

	attr.name       = "mid";
	attr.entry      = mid_thread_entry;
	attr.priority   = 2;
	ulmk_thread_create(&attr);

	attr.name       = "low";
	attr.entry      = low_thread_entry;
	attr.priority   = 3;
	ulmk_thread_create(&attr);

	/*
	 * Yield: root (prio=200) re-enqueues itself and picks high (prio=1).
	 * Execution resumes here only after high, mid, and low all suspend.
	 */
	ulmk_thread_yield();

	ulmk_printk("sched_integ: PASS\n");
	ulmk_sim_exit(0);
}
