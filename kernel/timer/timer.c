/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * kernel/timer/timer.c — monotonic tick counter and hardware timer primitive.
 *
 * Monotonic time is maintained as a tick counter incremented once per
 * hardware tick interrupt.  ul_timer_now_us() converts ticks to microseconds
 * using the configured tick rate.
 *
 * Timer server primitive: the kernel exposes two operations:
 *   ul_timer_set_deadline() — records a tick deadline; protected by UL_CAP_TIMER.
 *   ul_timer_wait_thread()  — blocks the caller until that deadline expires.
 *
 * Only one thread (the timer server) may block here at a time.
 * ul_timer_tick() checks the deadline each tick and wakes the waiter when
 * it expires.  Sleep policy (sorted queues, multiple waiters) lives in
 * userspace inside the timer server.
 *
 * The hardware timer is periodic — no re-arming from this layer.
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/config.h>
#include <kernel/include/ul_timer_internal.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>
#include <ul_arch.h>

#define TICK_PERIOD_US	(1000000u / UL_CONFIG_TICK_HZ)

static volatile uint32_t g_tick_count;
static uint32_t          g_timer_deadline_ticks;
static ul_thread_t      *g_timer_waiter;

void ul_timer_init(void)
{
	g_tick_count           = 0u;
	g_timer_deadline_ticks = 0u;
	g_timer_waiter         = NULL;
}

uint32_t ul_timer_get_tick_count(void)
{
	return g_tick_count;
}

uint64_t ul_timer_now_us(void)
{
	return (uint64_t)g_tick_count * TICK_PERIOD_US;
}

void ul_timer_deadline_arm(uint64_t deadline_us)
{
	uint32_t ticks = (uint32_t)(deadline_us / TICK_PERIOD_US);

	if (ticks == 0u)
		ticks = 1u;
	g_timer_deadline_ticks = g_tick_count + ticks;
}

void ul_timer_wait_thread(ul_thread_t *cur)
{
	ul_arch_irq_key_t key;

	key = ul_arch_cpu_irq_save();
	cur->state          = UL_THREAD_STATE_BLOCKED;
	cur->blocked_reason = UL_BLOCKED_TIMER_WAIT;
	ul_sched_dequeue(cur);
	g_timer_waiter = cur;
	ul_arch_cpu_irq_restore(key);

	ul_sched_schedule();
}

/*
 * Cancel the timer wait for a specific thread (called when killing a
 * thread that is blocked in ul_timer_wait_thread()).
 */
void ul_timer_waiter_cancel(ul_thread_t *th)
{
	ul_arch_irq_key_t key;

	key = ul_arch_cpu_irq_save();
	if (g_timer_waiter == th)
		g_timer_waiter = NULL;
	ul_arch_cpu_irq_restore(key);
}

void ul_timer_tick(void)
{
	ul_arch_irq_key_t key;
	ul_thread_t      *waiter;

	g_tick_count++;

	if (!g_timer_waiter || g_tick_count < g_timer_deadline_ticks)
		return;

	key    = ul_arch_cpu_irq_save();
	waiter = g_timer_waiter;
	if (waiter) {
		g_timer_waiter = NULL;
		ul_sched_enqueue(waiter);
	}
	ul_arch_cpu_irq_restore(key);
}
