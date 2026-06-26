/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Scheduler integration test — tests/sched_integ/sched_integ_test.c
 *
 * Scenario: three threads (high/mid/low priority) + root.
 * Root lowers its own priority to 200, enqueues the three threads, yields.
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
#include <stddef.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_printk.h>

/* =========================================================================
 * Static thread control blocks and stacks
 * ========================================================================= */

static ul_thread_t high_thread;
static ul_thread_t mid_thread;
static ul_thread_t low_thread;

static uint8_t high_stack[1024] __attribute__((aligned(8)));
static uint8_t mid_stack[1024]  __attribute__((aligned(8)));
static uint8_t low_stack[1024]  __attribute__((aligned(8)));

/* =========================================================================
 * Thread entry points
 * ========================================================================= */

static void high_thread_entry(void *arg)
{
	(void)arg;
	ul_printk("sched_integ: high step 1\n");
	ul_thread_yield();
	/* Resumes here after yield when scheduler returns to us. */
	ul_printk("sched_integ: high step 2\n");
	ul_thread_suspend(ul_thread_self());
	for (;;)
		;
}

static void mid_thread_entry(void *arg)
{
	(void)arg;
	ul_printk("sched_integ: mid step 1\n");
	ul_thread_yield();
	ul_printk("sched_integ: mid step 2\n");
	ul_thread_suspend(ul_thread_self());
	for (;;)
		;
}

static void low_thread_entry(void *arg)
{
	(void)arg;
	ul_printk("sched_integ: low step 1\n");
	ul_thread_suspend(ul_thread_self());
	for (;;)
		;
}

/* =========================================================================
 * Root thread — provided as ul_root_thread() per boot model
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;

	(void)info;

	ul_printk("sched_integ: start\n");

	/*
	 * Lower root's priority so higher-priority test threads run first.
	 * Root was created with prio=0 by kernel_main.c; set it to 200.
	 */
	ul_thread_priority_set(ul_thread_self(), 200);

	/* Create and enqueue high-priority thread */
	attr.name      = "high";
	attr.entry     = high_thread_entry;
	attr.arg       = NULL;
	attr.priority  = 1;
	attr.stack_size = sizeof(high_stack);
	attr.privilege = UL_PRIV_DRIVER;
	ul_thread_init(&high_thread, &attr, high_stack);
	ul_sched_enqueue(&high_thread);

	/* Create and enqueue mid-priority thread */
	attr.name      = "mid";
	attr.entry     = mid_thread_entry;
	attr.arg       = NULL;
	attr.priority  = 2;
	ul_thread_init(&mid_thread, &attr, mid_stack);
	ul_sched_enqueue(&mid_thread);

	/* Create and enqueue low-priority thread */
	attr.name      = "low";
	attr.entry     = low_thread_entry;
	attr.arg       = NULL;
	attr.priority  = 3;
	ul_thread_init(&low_thread, &attr, low_stack);
	ul_sched_enqueue(&low_thread);

	/*
	 * Yield: root (prio=200) re-enqueues itself and picks high (prio=1).
	 * Execution resumes here only after high, mid, and low all suspend.
	 */
	ul_thread_yield();

	/* Reached when all test threads are blocked and root is picked. */
	ul_printk("sched_integ: PASS\n");
	ul_thread_exit();
}
