/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Kernel scheduler internal interface.
 * Implements fixed-priority round-robin.
 */

#ifndef UL_SCHED_H
#define UL_SCHED_H

#include <kernel/include/ul_thread_internal.h>

void         ul_sched_init(void);
void         ul_sched_enqueue(ul_thread_t *th);
void         ul_sched_dequeue(ul_thread_t *th);
ul_thread_t *ul_sched_pick_next(void);
ul_thread_t *ul_sched_current(void);
void         ul_sched_tick(void);

#endif /* UL_SCHED_H */
