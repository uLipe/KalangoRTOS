/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Scheduler core — kernel/sched/sched.c
 * Dispatches through ul_sched_class_t vtable; manages current/idle state.
 */

#include <stddef.h>
#include <stdbool.h>
#include <ul/config.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_mem_internal.h>
#include <ul_arch.h>

static const ul_sched_class_t	*sched_class;
static ul_arch_ctx_t		*sched_idle;
static ul_thread_t		*sched_current;

/*
 * Dead-thread reaper slot.
 *
 * When a thread calls ul_thread_exit() it is still on the CPU — its CSA
 * chain is live in the hardware PCXI register.  We cannot walk and free
 * that chain until after the context switch hands control to the next
 * thread.  ul_sched_set_dead_for_cleanup() stores the dying thread here;
 * ul_sched_schedule() frees the CSA chain and stack at the start of its
 * next invocation, at which point the dead thread's PCXI is saved in its
 * ctx and is safe to walk.
 *
 * Only one slot is needed: a thread can exit at most once, and the next
 * call to ul_sched_schedule() always drains the slot before any new exit
 * can enqueue another dying thread.
 */
static ul_thread_t *g_sched_dead;

/*
 * Preemption handoff pointers: written by ul_sched_tick() and consumed by
 * the tick ISR assembly stub (_arch_tick_preempt_isr) in vectors.S.
 * The stub reads them after the C handler returns, at which point
 * PCXI = L_sv -> U_hw (the interrupted thread's ISR context chain).
 * Setting g_preempt_new_ctx != NULL signals: "perform a preemptive switch".
 */
ul_arch_ctx_t *g_preempt_old_ctx;
ul_arch_ctx_t *g_preempt_new_ctx;

/*
 * Register a dying thread for deferred CSA/stack cleanup.
 *
 * If a thread is already pending (g_sched_dead != NULL), it is reaped
 * immediately before recording the new one.  This handles the case
 * where threads exit in rapid succession without another thread
 * interleaving: the previous dead thread has already been context-
 * switched out (otherwise we would not be executing in the new one),
 * so its CSA chain is fully saved and safe to walk and free.
 *
 * Called with interrupts enabled; the FCX modification in
 * ul_arch_ctx_free() is guarded by a local irq_save/restore.
 */
void ul_sched_set_dead_for_cleanup(ul_thread_t *th)
{
	ul_arch_irq_key_t key;

	key = ul_arch_cpu_irq_save();
	if (g_sched_dead) {
		ul_arch_ctx_free(&g_sched_dead->ctx);
		if (g_sched_dead->stack_base)
			ul_phys_free(g_sched_dead->stack_base);
		ul_thread_pool_free(g_sched_dead);
	}
	g_sched_dead = th;
	ul_arch_cpu_irq_restore(key);
}

void ul_sched_init(ul_arch_ctx_t *idle)
{
	sched_idle    = idle;
	sched_current = NULL;
	sched_class   = NULL;
}

void ul_sched_set_class(const ul_sched_class_t *cls)
{
	sched_class = cls;
	cls->init();
}

/*
 * ul_sched_start — perform the first context switch from idle to a thread.
 *
 * Records first as current, configures MPU for the first thread, then
 * switches the idle context (kernel_main's stack) to the thread.
 */
void ul_sched_start(ul_arch_ctx_t *idle, ul_thread_t *first)
{
	sched_idle           = idle;
	sched_current        = first;
	first->state         = UL_THREAD_STATE_RUNNING;
	ul_arch_mpu_switch(first->regions, first->region_count, 1u);
	ul_arch_ctx_switch(idle, &first->ctx);
}

/*
 * ul_sched_schedule — switch to the highest-priority ready thread.
 *
 * If the run queue is empty, switches to the idle context.  If pick_next
 * returns the same thread that is already running (only one ready thread
 * of that priority after a yield), the switch is still performed so the
 * CSA chain unwinds correctly through ul_arch_ctx_switch.
 */
void ul_sched_schedule(void)
{
	ul_thread_t      *prev = sched_current;
	ul_thread_t      *next;
	ul_arch_ctx_t    *from;
	ul_arch_ctx_t    *to;
	ul_arch_irq_key_t key;

	/*
	 * Release CSA chain and stack of a previously-dead thread.
	 *
	 * The guard `g_sched_dead != prev` is critical: when a thread calls
	 * ul_kern_exit() it sets g_sched_dead = self and then immediately
	 * calls ul_sched_schedule().  At that point the thread's CSA chain
	 * is still live — ul_arch_ctx_switch() is about to execute svlcx,
	 * which saves the current lower context into a free CSA frame and
	 * then records the resulting PCXI into g_sched_dead->ctx.pcxi.
	 * Freeing the chain before that svlcx would return those frames to
	 * FCX, only for svlcx to immediately re-allocate and corrupt them.
	 *
	 * By skipping cleanup when g_sched_dead == prev we defer to the
	 * NEXT invocation of ul_sched_schedule() from any other thread, at
	 * which point the context switch has completed and the dead thread's
	 * PCXI chain is fully recorded and no longer referenced by the CPU.
	 */
	if (g_sched_dead && g_sched_dead != prev) {
		key = ul_arch_cpu_irq_save();
		ul_arch_ctx_free(&g_sched_dead->ctx);
		if (g_sched_dead->stack_base)
			ul_phys_free(g_sched_dead->stack_base);
		ul_thread_pool_free(g_sched_dead);
		g_sched_dead = NULL;
		ul_arch_cpu_irq_restore(key);
	}

	next = sched_class->pick_next();

	from = prev ? &prev->ctx : sched_idle;

	if (next) {
		sched_current        = next;
		next->state          = UL_THREAD_STATE_RUNNING;
		next->ticks_remaining = UL_CONFIG_SCHED_QUANTUM_TICKS;
		to = &next->ctx;
		ul_arch_mpu_switch(next->regions, next->region_count, 1u);
	} else {
		sched_current = NULL;
		to = sched_idle;
		ul_arch_mpu_switch(NULL, 0u, 1u);
	}

	if (from == to)
		return;

	ul_arch_ctx_switch(from, to);
}

void ul_sched_enqueue(ul_thread_t *t)
{
	sched_class->enqueue(t);
}

void ul_sched_dequeue(ul_thread_t *t)
{
	sched_class->dequeue(t);
}

ul_thread_t *ul_sched_current(void)
{
	return sched_current;
}

ul_thread_t *ul_sched_pick_next(void)
{
	return sched_class->pick_next();
}

void ul_sched_tick(void)
{
	ul_thread_t *cur = sched_current;
	ul_thread_t *next;
	bool         need_preempt;

	if (!cur || !sched_class)
		return;

	/*
	 * Skip if the current thread is already leaving the CPU
	 * (BLOCKED, DEAD, etc.).  A syscall like ul_kern_sleep_us sets
	 * state = BLOCKED then calls ul_sched_schedule(), but the tick ISR
	 * can fire in between.  Without this guard, the enqueue inside
	 * the rotation below would call fifo_rt_enqueue() which resets
	 * state = READY, corrupting the sleep-queue entry.
	 */
	if (cur->state != UL_THREAD_STATE_RUNNING)
		return;

	/* Decrement time-slice counter */
	if (cur->ticks_remaining > 0)
		cur->ticks_remaining--;

	/*
	 * Trigger preemption when the quantum expires OR when a strictly
	 * higher-priority thread (e.g. woken by ul_timer_tick) is waiting.
	 */
	next         = sched_class->peek_next();
	need_preempt = (cur->ticks_remaining == 0) ||
	               (next && next != cur && next->priority < cur->priority);

	if (!need_preempt)
		return;

	/* Reset quantum for the next run of cur */
	cur->ticks_remaining = UL_CONFIG_SCHED_QUANTUM_TICKS;

	/* Rotate cur to tail of its priority group (FIFO round-robin) */
	ul_sched_dequeue(cur);
	ul_sched_enqueue(cur);

	/* Pick the best thread now that cur is at the tail */
	next = sched_class->pick_next();
	if (!next || next == cur) {
		/* cur is the only candidate — keep it running */
		cur->state    = UL_THREAD_STATE_RUNNING;
		sched_current = cur;
		return;
	}

	/*
	 * Signal the ISR stub to perform the preemptive context switch.
	 * The stub reads g_preempt_new_ctx after _arch_tick_isr_handler
	 * returns, when PCXI = L_sv -> U_hw (the interrupted thread's
	 * ISR context chain).  It saves that chain to g_preempt_old_ctx->pcxi
	 * and loads g_preempt_new_ctx->pcxi, then does rslcx + rfe.
	 */
	sched_current         = next;
	next->state           = UL_THREAD_STATE_RUNNING;
	next->ticks_remaining = UL_CONFIG_SCHED_QUANTUM_TICKS;
	ul_arch_mpu_switch(next->regions, next->region_count, 1u);
	g_preempt_old_ctx = &cur->ctx;
	g_preempt_new_ctx = &next->ctx;
}
