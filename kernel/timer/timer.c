/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * kernel/timer/timer.c — tickless sleep queue
 *
 * Monotonic time: 64-bit µs built from the 32-bit ul_arch_tick_get()
 * counter.  Wrap events (every ~4294 s) are detected by comparing the
 * current reading against the last known value; ul_timer_now_us() must
 * be called at least once per wrap period to stay accurate.
 *
 * Sleep queue: singly-linked list sorted by sleep_until ascending.
 * The hardware is armed (via ul_arch_tick_deadline) for the nearest
 * expiry; subsequent entries are chained and armed as each fires.
 *
 * ISR wakeup model (cooperative + idle-preempt):
 *   - If a thread is currently running, woken threads are enqueued and
 *     will be scheduled at the next voluntary yield/sleep.
 *   - If the CPU is idle (sched_current == NULL), ul_sched_schedule()
 *     is called directly from the ISR to switch to the woken thread.
 *     This is safe on TriCore: the idle CSA chain includes the ISR
 *     frames and unwinds correctly when idle is eventually resumed.
 */

#include <stdint.h>
#include <stddef.h>
#include <kernel/include/ul_timer_internal.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>
#include <ul_arch.h>

static ul_thread_t	*sleep_head;
static uint32_t		 g_last_stm;
static uint64_t		 g_mono_us;

void ul_timer_init(void)
{
	g_last_stm = ul_arch_tick_get();
}

uint64_t ul_timer_now_us(void)
{
	uint32_t now = ul_arch_tick_get();

	if (now < g_last_stm) {
		/* 32-bit wrap: add the distance to 2^32 then the new value. */
		g_mono_us += (uint64_t)(UINT32_MAX - g_last_stm) + (uint64_t)now + 1u;
	} else {
		g_mono_us += (uint64_t)(now - g_last_stm);
	}
	g_last_stm = now;
	return g_mono_us;
}

static void arm_nearest(uint64_t now)
{
	uint64_t delta;
	uint32_t delta32;

	if (!sleep_head)
		return;

	delta = (sleep_head->sleep_until > now) ?
		(sleep_head->sleep_until - now) : 0u;

	delta32 = (delta > (uint64_t)UINT32_MAX) ? UINT32_MAX : (uint32_t)delta;
	ul_arch_tick_deadline(delta32 ? delta32 : 1u);
}

void ul_timer_sleep_insert(ul_thread_t *th, uint64_t deadline_us)
{
	ul_thread_t **pp = &sleep_head;

	while (*pp && (*pp)->sleep_until <= deadline_us)
		pp = &(*pp)->sleep_next;

	th->sleep_until = deadline_us;
	th->sleep_next  = *pp;
	*pp = th;

	if (sleep_head == th)
		arm_nearest(ul_timer_now_us());
}

void ul_timer_tick(void)
{
	uint64_t	 now = ul_timer_now_us();
	ul_thread_t	*th;
	int		 woke_any = 0;

	while (sleep_head && sleep_head->sleep_until <= now) {
		th         = sleep_head;
		sleep_head = th->sleep_next;
		th->sleep_next = NULL;
		ul_sched_enqueue(th);
		woke_any = 1;
	}

	arm_nearest(now);

	/*
	 * Trigger a reschedule only when the CPU was idle.  A running thread
	 * will pick up the woken threads at its next voluntary yield/sleep.
	 */
	if (woke_any && ul_sched_current() == NULL)
		ul_sched_schedule();
}
