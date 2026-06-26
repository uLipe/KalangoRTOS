/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Kernel scheduler internal interface — kernel/include/ul_sched.h
 */

#ifndef UL_SCHED_H
#define UL_SCHED_H

#include <kernel/include/ul_thread_internal.h>
#include <ul_arch.h>

/* =========================================================================
 * Scheduler class — vtable for pluggable scheduling policies.
 *
 * All callbacks are invoked with interrupts serialised (cooperative OS:
 * only from syscall context or at boot before the first switch).
 *
 * pick_next() returns NULL when no thread is ready → idle.
 * ========================================================================= */

typedef struct ul_sched_class {
	const char	*name;
	void		 (*init)(void);
	void		 (*enqueue)(ul_thread_t *t);
	void		 (*dequeue)(ul_thread_t *t);
	ul_thread_t	*(*pick_next)(void);
} ul_sched_class_t;

/* Exported by kernel/sched/fifo_rt.c */
extern const ul_sched_class_t ul_fifo_rt_class;

/* =========================================================================
 * Scheduler core (kernel/sched/sched.c)
 * ========================================================================= */

/*
 * ul_sched_init — register the idle context; call before set_class/start.
 * ul_sched_set_class — install a scheduling policy and run its init hook.
 * ul_sched_start — record first thread as current, perform initial switch.
 *                  Does not return until something switches back to idle.
 * ul_sched_schedule — pick next ready thread and switch to it (or idle).
 */
void		 ul_sched_init(ul_arch_ctx_t *idle);
void		 ul_sched_set_class(const ul_sched_class_t *cls);
void		 ul_sched_start(ul_arch_ctx_t *idle, ul_thread_t *first);
void		 ul_sched_schedule(void);

void		 ul_sched_enqueue(ul_thread_t *t);
void		 ul_sched_dequeue(ul_thread_t *t);
ul_thread_t	*ul_sched_current(void);
ul_thread_t	*ul_sched_pick_next(void);
void		 ul_sched_tick(void);

#endif /* UL_SCHED_H */
