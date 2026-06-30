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
 * last one prints the PASS sentinel before calling ul_sim_exit(0).
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
#include "../test_support.h"
#include <stddef.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>


#define SLEEP_COUNT	50u
#define SLEEP_MS	10u		/* 10 ms per sleep */

/* =========================================================================
 * Shared state — updated cooperatively; no atomics needed.
 * ========================================================================= */

static uint32_t g_done_count;

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
		ul_sim_exit(0);
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
		ul_sim_exit(0);
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
		ul_sim_exit(0);
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

	attr.arg       = NULL;
	attr.privilege = UL_PRIV_DRIVER;

	attr.name       = "a";
	attr.entry      = thread_a_entry;
	attr.priority   = 1;
	attr.stack_size = 2048;
	ul_thread_create(&attr);

	attr.name       = "b";
	attr.entry      = thread_b_entry;
	attr.priority   = 2;
	ul_thread_create(&attr);

	attr.name       = "c";
	attr.entry      = thread_c_entry;
	attr.priority   = 3;
	ul_thread_create(&attr);

	ul_thread_exit();
}
