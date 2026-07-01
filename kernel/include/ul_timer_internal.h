/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal timer/sleep subsystem interface.
 * Not part of the public API — do not include from userspace.
 *
 * Design: periodic tick-based monotonic time.  ul_timer_now_us() returns
 * the current time in µs derived from the hardware tick counter;
 * resolution is one tick period (1 000 000 / UL_CONFIG_TICK_HZ µs).
 */

#ifndef UL_TIMER_INTERNAL_H
#define UL_TIMER_INTERNAL_H

#include <stdint.h>
#include <kernel/include/ul_thread_internal.h>

/*
 * ul_timer_init — initialise tick counter and sleep queue.
 * Must be called after ul_arch_tick_init() and before any ul_timer_* call.
 */
void ul_timer_init(void);

/*
 * ul_timer_get_tick_count — number of tick interrupts since boot.
 */
uint32_t ul_timer_get_tick_count(void);

/*
 * ul_timer_now_us — monotonic µs elapsed since boot.
 * Safe to call from ISR or thread context.
 */
uint64_t ul_timer_now_us(void);

/*
 * ul_timer_sleep_insert — block @th until @deadline_us µs (monotonic).
 * Inserts into the sorted sleep queue.
 * Caller must set th->state = BLOCKED and dequeue before calling.
 */
void ul_timer_sleep_insert(ul_thread_t *th, uint64_t deadline_us);

/*
 * ul_timer_sleep_remove — remove @th from the sleep queue without waking it.
 * No-op if @th is not currently in the queue.
 */
void ul_timer_sleep_remove(ul_thread_t *th);

/*
 * ul_timer_tick — called from ul_kernel_tick() (timer ISR context).
 * Increments the tick counter and expires sleeping threads.
 */
void ul_timer_tick(void);

#endif /* UL_TIMER_INTERNAL_H */
