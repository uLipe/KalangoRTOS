/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal timer/sleep subsystem interface.
 * Not part of the public API — do not include from userspace.
 *
 * Design: tickless, µs-resolution monotonic time derived from a 32-bit
 * free-running hardware counter (ul_arch_tick_get).  Wrap events are
 * detected in software — ul_timer_now_us() must be called at least
 * once every ~71 minutes to guarantee correct wrap accounting.
 */

#ifndef UL_TIMER_INTERNAL_H
#define UL_TIMER_INTERNAL_H

#include <stdint.h>
#include <kernel/include/ul_thread_internal.h>

/*
 * ul_timer_init — snapshot the initial hardware counter.
 * Must be called after ul_arch_tick_init() and before any ul_timer_* call.
 */
void ul_timer_init(void);

/*
 * ul_timer_now_us — 64-bit monotonic µs elapsed since boot.
 * Detects 32-bit hardware counter wrap; safe to call from ISR context.
 */
uint64_t ul_timer_now_us(void);

/*
 * ul_timer_sleep_insert — block @th until @deadline_us µs (monotonic).
 * Inserts into the sorted sleep queue and arms the hardware deadline if
 * @th becomes the new nearest-expiring entry.
 * Caller is responsible for setting th->state = BLOCKED and removing
 * the thread from the run queue before calling this.
 */
void ul_timer_sleep_insert(ul_thread_t *th, uint64_t deadline_us);

/*
 * ul_timer_sleep_remove — remove @th from the sleep queue without waking it.
 * No-op if @th is not currently in the queue.
 * Must be called with interrupts disabled (or from syscall context).
 */
void ul_timer_sleep_remove(ul_thread_t *th);

/*
 * ul_timer_tick — called from ul_kernel_tick() (timer ISR context).
 * Expires all threads whose deadline has passed, re-arms the hardware
 * for the next entry, and triggers ul_sched_schedule() when the CPU
 * was idle and at least one thread was woken.
 */
void ul_timer_tick(void);

#endif /* UL_TIMER_INTERNAL_H */
