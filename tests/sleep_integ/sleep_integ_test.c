/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Sleep syscall integration test — tests/sleep_integ/sleep_integ_test.c
 *
 * Scenario: three threads (A prio=1, B prio=2, C prio=3) each call
 * ul_msleep(10) fifty times.  The kernel timer ISR wakes sleeping threads
 * and calls ul_sched_schedule() when the CPU is idle, which drives the
 * context switches back to the threads.
 *
 * Expected outcome: all three threads complete their 50 sleeps and the
 * last one prints the PASS sentinel before calling qemu_virt_exit(0).
 *
 * Sentinels verified by the Makefile:
 *   sleep_integ: start
 *   sleep_integ: thread A alive (sleep 10)
 *   sleep_integ: thread A done
 *   sleep_integ: thread B done
 *   sleep_integ: thread C done
 *   sleep_integ: PASS
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_printk.h>

extern void qemu_virt_exit(uint32_t code);

#define SLEEP_COUNT	50u
#define SLEEP_MS	10u		/* 10 ms per sleep */

/* =========================================================================
 * Shared state — updated cooperatively; no atomics needed.
 * ========================================================================= */

static uint32_t g_done_count;

/* =========================================================================
 * Static TCBs and stacks
 * ========================================================================= */

static ul_thread_t a_thread;
static ul_thread_t b_thread;
static ul_thread_t c_thread;

static uint8_t a_stack[2048] __attribute__((aligned(8)));
static uint8_t b_stack[2048] __attribute__((aligned(8)));
static uint8_t c_stack[2048] __attribute__((aligned(8)));

/* =========================================================================
 * Thread entries
 * ========================================================================= */

static void thread_a_entry(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0; i < SLEEP_COUNT; i++) {
		ul_msleep(SLEEP_MS);
		if (i == 9u)
			ul_printk("sleep_integ: thread A alive (sleep 10)\n");
	}
	ul_printk("sleep_integ: thread A done\n");

	g_done_count++;
	if (g_done_count == 3u) {
		ul_printk("sleep_integ: PASS\n");
		qemu_virt_exit(0);
	}
	ul_thread_exit();
}

static void thread_b_entry(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0; i < SLEEP_COUNT; i++)
		ul_msleep(SLEEP_MS);

	ul_printk("sleep_integ: thread B done\n");

	g_done_count++;
	if (g_done_count == 3u) {
		ul_printk("sleep_integ: PASS\n");
		qemu_virt_exit(0);
	}
	ul_thread_exit();
}

static void thread_c_entry(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0; i < SLEEP_COUNT; i++)
		ul_msleep(SLEEP_MS);

	ul_printk("sleep_integ: thread C done\n");

	g_done_count++;
	if (g_done_count == 3u) {
		ul_printk("sleep_integ: PASS\n");
		qemu_virt_exit(0);
	}
	ul_thread_exit();
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;

	(void)info;

	ul_printk("sleep_integ: start\n");

	/*
	 * Lower root priority so the three test threads run before root
	 * blocks waiting (root just exits immediately after spawning).
	 */
	ul_thread_priority_set(ul_thread_self(), 200);

	attr.name       = "a";
	attr.entry      = thread_a_entry;
	attr.arg        = NULL;
	attr.priority   = 1;
	attr.stack_size = sizeof(a_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_init(&a_thread, &attr, a_stack);
	ul_sched_enqueue(&a_thread);

	attr.name       = "b";
	attr.entry      = thread_b_entry;
	attr.arg        = NULL;
	attr.priority   = 2;
	ul_thread_init(&b_thread, &attr, b_stack);
	ul_sched_enqueue(&b_thread);

	attr.name       = "c";
	attr.entry      = thread_c_entry;
	attr.arg        = NULL;
	attr.priority   = 3;
	ul_thread_init(&c_thread, &attr, c_stack);
	ul_sched_enqueue(&c_thread);

	ul_thread_exit();
}
