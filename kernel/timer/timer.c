/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * kernel/timer/timer.c — periodic tick-based sleep queue
 *
 * Monotonic time is maintained as a tick counter incremented once per
 * hardware tick interrupt.  ul_timer_now_us() converts this to
 * microseconds using the configured tick rate; precision is ±1 tick.
 *
 * Sleep queue: singly-linked list sorted by sleep_until (absolute µs)
 * ascending.  ul_timer_tick() is called from the tick ISR via
 * ul_kernel_tick() and expires all threads whose deadline has passed.
 *
 * The hardware timer is periodic — no re-arming from this layer.
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/config.h>
#include <kernel/include/ul_timer_internal.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>

#define TICK_PERIOD_US	(1000000u / UL_CONFIG_TICK_HZ)

static ul_thread_t	*sleep_head;
static volatile uint32_t g_tick_count;

void ul_timer_init(void)
{
	g_tick_count = 0;
	sleep_head   = NULL;
}

uint32_t ul_timer_get_tick_count(void)
{
	return g_tick_count;
}

uint64_t ul_timer_now_us(void)
{
	return (uint64_t)g_tick_count * TICK_PERIOD_US;
}

void ul_timer_sleep_insert(ul_thread_t *th, uint64_t deadline_us)
{
	ul_thread_t **pp = &sleep_head;

	while (*pp && (*pp)->sleep_until <= deadline_us)
		pp = &(*pp)->sleep_next;

	th->sleep_until = deadline_us;
	th->sleep_next  = *pp;
	*pp = th;
}

void ul_timer_sleep_remove(ul_thread_t *th)
{
	ul_thread_t **pp = &sleep_head;

	while (*pp && *pp != th)
		pp = &(*pp)->sleep_next;

	if (!*pp)
		return;

	*pp = th->sleep_next;
	th->sleep_next  = NULL;
	th->sleep_until = 0u;
}

void ul_timer_tick(void)
{
	uint64_t	 now;
	ul_thread_t	*th;

	g_tick_count++;
	now = (uint64_t)g_tick_count * TICK_PERIOD_US;

	while (sleep_head && sleep_head->sleep_until <= now) {
		th         = sleep_head;
		sleep_head = th->sleep_next;
		th->sleep_next = NULL;
		ul_sched_enqueue(th);
	}

}
