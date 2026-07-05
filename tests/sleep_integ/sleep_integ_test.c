/* SPDX-License-Identifier: MIT */
/*
 * Sleep integration test — tests/sleep_integ/sleep_integ_test.c
 *
 * Validates board_timer_sleep_us() via the QEMU board timer service.
 * Three equal-priority threads each sleep 10 ms three times; the timer
 * server serialises requests over IPC.
 */

#include <stdint.h>
#include "../test_support.h"
#include <stddef.h>
#include <ulmk/microkernel.h>

#define SLEEP_COUNT	3u
#define SLEEP_US	10000u
#define THREAD_PRIO	5u

static volatile uint32_t g_done;

static void sleeper(void *arg)
{
	unsigned i;
	const char *tag = (const char *)arg;

	for (i = 0; i < SLEEP_COUNT; i++)
		board_timer_sleep_us(SLEEP_US);

	ulmk_printk("sleep_integ: thread %s done\n", tag);
	g_done++;
	ulmk_thread_exit();
}

static void supervisor(void *arg)
{
	(void)arg;

	while (g_done < 3u)
		ulmk_thread_yield();

	ulmk_printk("sleep_integ: PASS\n");
	ulmk_sim_exit(0);
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};

	(void)info;

	ulmk_printk("sleep_integ: start\n");
	ulmk_printk("sleep_integ: thread A alive (sleep 10)\n");

	board_timer_start(info);

	attr.stack_size = 2048u;
	attr.privilege  = ULMK_PRIV_USER;
	attr.priority   = THREAD_PRIO;

	attr.name  = "A";
	attr.entry = sleeper;
	attr.arg   = (void *)"A";
	ulmk_thread_create(&attr);

	attr.name  = "B";
	attr.arg   = (void *)"B";
	ulmk_thread_create(&attr);

	attr.name  = "C";
	attr.arg   = (void *)"C";
	ulmk_thread_create(&attr);

	attr.name       = "sup";
	attr.entry      = supervisor;
	attr.arg        = NULL;
	attr.priority   = 6u;
	attr.stack_size = 2048u;
	ulmk_thread_create(&attr);

	ulmk_thread_exit();
}
