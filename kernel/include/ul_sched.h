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
 * All callbacks are invoked with interrupts serialised.
 *
 * pick_next() / peek_next() return the highest-priority ready thread.
 * The idle thread (lowest priority sentinel) is always in the run queue,
 * so these never return NULL after ul_sched_start().
 * ========================================================================= */

typedef struct ul_sched_class {
	const char	*name;
	void		 (*init)(void);
	void		 (*enqueue)(ul_thread_t *t);
	void		 (*dequeue)(ul_thread_t *t);
	ul_thread_t	*(*pick_next)(void);
	ul_thread_t	*(*peek_next)(void);
} ul_sched_class_t;

/* Exported by kernel/sched/fifo_rt.c */
extern const ul_sched_class_t ul_fifo_rt_class;

/*
 * Preemption handoff: set by ul_sched_tick() or ul_sched_check_preempt()
 * when a preemptive switch is needed.  Consumed by ISR assembly stubs
 * (_arch_tick_preempt_isr, _arch_generic_preempt_isr) after the C handler
 * returns, at which point PCXI = L_sv -> U_hw and the switch can be
 * performed without an extra svlcx.
 * Both are NULL when no preemption is pending.
 */
extern ul_arch_ctx_t *g_preempt_old_ctx;
extern ul_arch_ctx_t *g_preempt_new_ctx;

/* =========================================================================
 * Scheduler core (kernel/sched/sched.c)
 * ========================================================================= */

/*
 * ul_sched_init     — initialise scheduler state; call before set_class/start.
 * ul_sched_set_class — install a scheduling policy and run its init hook.
 * ul_sched_start    — perform the first context switch from the startup frame
 *                     to the highest-priority ready thread.  Does not return.
 * ul_sched_schedule — pick the highest-priority ready thread and switch to it.
 *                     The caller must have already arranged for the current
 *                     thread to be either blocked/dead (dequeued) or READY
 *                     (in queue) before calling.
 */
void		 ul_sched_init(void);
void		 ul_sched_set_class(const ul_sched_class_t *cls);
void		 ul_sched_start(void);
void		 ul_sched_schedule(void);

void		 ul_sched_enqueue(ul_thread_t *t);
void		 ul_sched_dequeue(ul_thread_t *t);
ul_thread_t	*ul_sched_current(void);
ul_thread_t	*ul_sched_peek_next(void);

/*
 * ul_sched_tick — called from the tick ISR.  Decrements the running thread's
 * quantum and arms the ISR preemption handoff if a switch is needed.
 *
 * ul_sched_check_preempt — called from a generic ISR after a notification is
 * delivered.  If a strictly higher-priority thread became ready, arms the
 * g_preempt_old/new_ctx handoff so the ISR stub performs a context switch on
 * exit (same mechanism as ul_sched_tick).
 * Must be called with interrupts disabled (already the case inside an ISR).
 */
void		 ul_sched_tick(void);
void		 ul_sched_check_preempt(void);

/*
 * ul_sched_set_dead_for_cleanup — register a dead thread for deferred
 * resource release.  Called from ul_kern_exit() and ul_kern_thread_kill()
 * when the target is the currently-running thread; in that case the CSA
 * chain is still active in PCXI and must not be freed until after the
 * next context switch.  ul_sched_schedule() performs the actual release
 * at its next invocation, when the dead thread is no longer on the CPU.
 */
void		 ul_sched_set_dead_for_cleanup(ul_thread_t *th);

#endif /* UL_SCHED_H */
