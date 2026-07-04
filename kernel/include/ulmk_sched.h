/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Kernel scheduler internal interface — kernel/include/ulmk_sched.h
 */

#ifndef UL_SCHED_H
#define UL_SCHED_H

#include <kernel/include/ulmk_thread_internal.h>
#include <ulmk_arch.h>

/* =========================================================================
 * Scheduler class — vtable for pluggable scheduling policies.
 *
 * All callbacks are invoked with interrupts serialised.
 *
 * pick_next() / peek_next() return the highest-priority ready thread.
 * The idle thread (lowest priority sentinel) is always in the run queue,
 * so these never return NULL after ulmk_sched_start().
 * ========================================================================= */

typedef struct ulmk_sched_class {
	const char	*name;
	void		 (*init)(void);
	void		 (*enqueue)(ulmk_thread_t *t);
	void		 (*dequeue)(ulmk_thread_t *t);
	ulmk_thread_t	*(*pick_next)(void);
	ulmk_thread_t	*(*peek_next)(void);
} ulmk_sched_class_t;

/* Exported by kernel/sched/fifo_rt.c */
extern const ulmk_sched_class_t ulmk_fifo_rt_class;

/* Exported by kernel/sched/bitmap_rt.c — 256-level O(1) scheduler (default) */
extern const ulmk_sched_class_t ulmk_bitmap_rt_class;

/*
 * Preemption handoff: set by ulmk_sched_check_preempt() when a strictly
 * higher-priority thread becomes ready.  Consumed by the arch ISR stubs
 * after the C handler returns, before context restore, so the switch can
 * be performed without an extra save instruction.
 * Both are NULL when no preemption is pending.
 */
extern ulmk_arch_ctx_t *g_preempt_old_ctx;
extern ulmk_arch_ctx_t *g_preempt_new_ctx;

/* =========================================================================
 * Scheduler core (kernel/sched/sched.c)
 * ========================================================================= */

/*
 * ulmk_sched_init     — initialise scheduler state; call before set_class/start.
 * ulmk_sched_set_class — install a scheduling policy and run its init hook.
 * ulmk_sched_start    — perform the first context switch from the startup frame
 *                     to the highest-priority ready thread.  Does not return.
 * ulmk_sched_schedule — pick the highest-priority ready thread and switch to it.
 *                     The caller must have already arranged for the current
 *                     thread to be either blocked/dead (dequeued) or READY
 *                     (in queue) before calling.
 */
void		 ulmk_sched_init(void);
void		 ulmk_sched_set_class(const ulmk_sched_class_t *cls);
void		 ulmk_sched_start(void);
void		 ulmk_sched_schedule(void);

void		 ulmk_sched_enqueue(ulmk_thread_t *t);
void		 ulmk_sched_dequeue(ulmk_thread_t *t);
ulmk_thread_t	*ulmk_sched_current(void);
ulmk_thread_t	*ulmk_sched_peek_next(void);

/*
 * ulmk_sched_check_preempt — called from a generic ISR after a notification is
 * delivered.  If a strictly higher-priority thread became ready, arms the
 * g_preempt_old/new_ctx handoff so the ISR stub performs a context switch on
 * exit.
 */
void		 ulmk_sched_check_preempt(void);

/*
 * ulmk_sched_set_dead_for_cleanup — register a dead thread for deferred
 * resource release.  Called from ulmk_kern_exit() and ulmk_kern_thread_kill()
 * when the target is the currently-running thread; in that case the context
 * chain is still active and must not be freed until after the next context
 * switch.  ulmk_sched_schedule() performs the actual release at its next
 * invocation, when the dead thread is no longer on the CPU.
 */
void		 ulmk_sched_set_dead_for_cleanup(ulmk_thread_t *th);

#endif /* UL_SCHED_H */
