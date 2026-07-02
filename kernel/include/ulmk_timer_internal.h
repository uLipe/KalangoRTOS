/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal timer subsystem interface.
 * Not part of the public API — do not include from userspace.
 *
 * Design: periodic tick-based monotonic time + single hardware timer primitive.
 * Sleep policy (sorted queues, multiple waiters) lives in the userspace timer
 * server built on ulmk_timer_set_deadline() and ulmk_timer_wait_thread().
 */

#ifndef UL_TIMER_INTERNAL_H
#define UL_TIMER_INTERNAL_H

#include <stdint.h>
#include <kernel/include/ulmk_thread_internal.h>

/*
 * ulmk_timer_init — initialise tick counter and timer primitive state.
 * Must be called after ulmk_arch_tick_init() and before any ulmk_timer_* call.
 */
void ulmk_timer_init(void);

/*
 * ulmk_timer_get_tick_count — number of tick interrupts since boot.
 */
uint32_t ulmk_timer_get_tick_count(void);

/*
 * ulmk_timer_now_us — monotonic µs elapsed since boot.
 * Safe to call from ISR or thread context.
 */
uint64_t ulmk_timer_now_us(void);

/*
 * ulmk_timer_deadline_arm — program the next timer expiry.
 * deadline_us: absolute monotonic microseconds from now.
 * Protected by ULMK_CAP_TIMER; only the timer server may call this via syscall.
 */
void ulmk_timer_deadline_arm(uint64_t deadline_us);

/*
 * ulmk_timer_wait_thread — block @cur until the programmed deadline expires.
 * @cur must be the currently running thread.
 * Exactly one thread may be waiting at a time (the timer server).
 * Returns when ulmk_timer_tick() determines the deadline has passed.
 */
void ulmk_timer_wait_thread(ulmk_thread_t *cur);

/*
 * ulmk_timer_waiter_cancel — remove @th from the timer waiter slot.
 * Called by ulmk_kern_thread_kill() when the target is the timer waiter.
 */
void ulmk_timer_waiter_cancel(ulmk_thread_t *th);

/*
 * ulmk_timer_tick — called from ulmk_kern_tick() (timer ISR context).
 * Increments the tick counter and wakes the timer waiter if the deadline
 * has expired.
 */
void ulmk_timer_tick(void);

#endif /* UL_TIMER_INTERNAL_H */
