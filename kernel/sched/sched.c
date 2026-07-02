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

static const ul_sched_class_t * UL_KERNEL_BSS sched_class;
static ul_thread_t            * UL_KERNEL_BSS sched_current;

/*
 * Dead-thread reaper slot.
 *
 * When a thread calls ul_thread_exit() it is still on the CPU — its
 * hardware context chain is live in the arch context register.  We cannot
 * walk and free that chain until after the context switch hands control to
 * the next thread.  ul_sched_set_dead_for_cleanup() stores the dying thread
 * here; ul_sched_schedule() frees the context chain and stack at the start
 * of its next invocation, at which point the dead thread's context pointer
 * is saved in its ctx and is safe to walk.
 *
 * Only one slot is needed: a thread can exit at most once, and the next
 * call to ul_sched_schedule() always drains the slot before any new exit
 * can enqueue another dying thread.
 */
static ul_thread_t * UL_KERNEL_BSS g_sched_dead;

/*
 * Preemption handoff pointers: written by ul_sched_tick() or
 * ul_sched_check_preempt() and consumed by ISR assembly stubs in the arch
 * layer.  The stub reads them after the C handler returns, before restoring
 * the interrupted thread's context.
 * Setting g_preempt_new_ctx != NULL signals: "perform a preemptive switch".
 */
ul_arch_ctx_t * UL_KERNEL_BSS g_preempt_old_ctx;
ul_arch_ctx_t * UL_KERNEL_BSS g_preempt_new_ctx;

/*
 * Saved at ul_sched_start() to provide a valid "from" context for the
 * initial switch out of the startup frame.  Written once; never loaded.
 */
static ul_arch_ctx_t UL_KERNEL_BSS startup_ctx;

/*
 * Register a dying thread for deferred context/stack cleanup.
 *
 * If a thread is already pending (g_sched_dead != NULL), it is reaped
 * immediately before recording the new one.  This handles the case
 * where threads exit in rapid succession without another thread
 * interleaving: the previous dead thread has already been context-
 * switched out (otherwise we would not be executing in the new one),
 * so its context chain is fully saved and safe to walk and free.
 *
 * Called with interrupts enabled; ul_arch_ctx_free() is guarded
 * internally by a local irq_save/restore.
 */
void ul_sched_set_dead_for_cleanup(ul_thread_t *th)
{
	ul_arch_irq_key_t key;

	key = ul_arch_cpu_irq_save();
	if (g_sched_dead) {
		ul_arch_ctx_free(&g_sched_dead->ctx);
		ul_thread_free(g_sched_dead);
	}
	g_sched_dead = th;
	ul_arch_cpu_irq_restore(key);
}

void ul_sched_init(void)
{
	sched_current = NULL;
	sched_class   = NULL;
}

void ul_sched_set_class(const ul_sched_class_t *cls)
{
	sched_class = cls;
	cls->init();
}

/*
 * ul_sched_start — perform the first context switch from the startup frame
 * to the highest-priority ready thread.
 *
 * The startup frame context (startup_ctx) is written once and never loaded
 * again; it acts purely as the "from" argument for the initial switch.
 * Since the idle thread is always in the run queue, pick_next() is
 * guaranteed to return a valid thread.  Does not return.
 */
void ul_sched_start(void)
{
	ul_thread_t *first = sched_class->pick_next();

	sched_current          = first;
	first->state           = UL_THREAD_STATE_RUNNING;
	first->ticks_remaining = UL_CONFIG_SCHED_QUANTUM_TICKS;
	ul_arch_mpu_switch(first->regions, first->region_count, 1u);
	ul_arch_ctx_switch(&startup_ctx, &first->ctx);
}

/*
 * ul_sched_schedule — switch to the highest-priority ready thread.
 *
 * The running thread must have already set its own state to BLOCKED/DEAD
 * (and dequeued itself) before calling, OR set it to READY (remaining in
 * the queue) if voluntarily yielding the CPU.
 *
 * With the idle thread permanently in the queue, pick_next() always
 * returns a valid thread, so there is no idle-context fallback path.
 */
void ul_sched_schedule(void)
{
	ul_thread_t      *prev = sched_current;
	ul_thread_t      *next;
	ul_arch_ctx_t    *from;
	ul_arch_ctx_t    *to;
	ul_arch_irq_key_t key;

	/*
	 * Release context chain and stack of a previously-dead thread.
	 *
	 * The guard `g_sched_dead != prev` is critical: when a thread calls
	 * ul_kern_exit() it sets g_sched_dead = self and then immediately
	 * calls ul_sched_schedule().  At that point the thread's context chain
	 * is still live — ul_arch_ctx_switch() is about to save the current
	 * lower context into a free frame and record the resulting context
	 * pointer into g_sched_dead->ctx.  Freeing the chain before that save
	 * would return those frames to the free list, only for the save
	 * instruction to immediately re-allocate and corrupt them.
	 */
	if (g_sched_dead && g_sched_dead != prev) {
		key = ul_arch_cpu_irq_save();
		ul_arch_ctx_free(&g_sched_dead->ctx);
		ul_thread_free(g_sched_dead);
		g_sched_dead = NULL;
		ul_arch_cpu_irq_restore(key);
	}

	next = sched_class->pick_next();
	from = prev ? &prev->ctx : &startup_ctx;

	/*
	 * Disable interrupts before committing sched_current = next.
	 *
	 * Between this assignment and the actual context pointer swap inside
	 * ul_arch_ctx_switch(), ul_sched_tick() could fire and see "next" as
	 * sched_current (state=RUNNING) while the CPU is still executing on
	 * "prev"'s stack.  The preemptive ISR would then save the wrong context
	 * pointer into next->ctx, corrupting its context chain.
	 */
	key = ul_arch_cpu_irq_save();

	sched_current          = next;
	next->state            = UL_THREAD_STATE_RUNNING;
	next->ticks_remaining  = UL_CONFIG_SCHED_QUANTUM_TICKS;
	to = &next->ctx;
	ul_arch_mpu_switch(next->regions, next->region_count, 1u);

	if (from == to) {
		ul_arch_cpu_irq_enable();
		return;
	}

	ul_arch_ctx_switch(from, to);
	/*
	 * We return here when THIS thread (prev) is re-scheduled.
	 *
	 * The arch context restore does not re-enable interrupts (IE was
	 * cleared by irq_save() before the switch).  Re-enable unconditionally.
	 */
	ul_arch_cpu_irq_enable();
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

ul_thread_t *ul_sched_peek_next(void)
{
	return sched_class ? sched_class->peek_next() : NULL;
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

	if (cur->ticks_remaining > 0)
		cur->ticks_remaining--;

	/*
	 * Trigger preemption when the quantum expires OR when a strictly
	 * higher-priority thread (e.g. woken by ul_timer_tick) is ready.
	 * peek_next() returns rq_head; if cur is at the head, next == cur
	 * and no priority preemption fires.
	 */
	next         = sched_class->peek_next();
	need_preempt = (cur->ticks_remaining == 0) ||
	               (next && next != cur && next->priority < cur->priority);

	if (!need_preempt)
		return;

	cur->ticks_remaining = UL_CONFIG_SCHED_QUANTUM_TICKS;

	/* Rotate cur to the tail of its priority group (FIFO round-robin) */
	ul_sched_dequeue(cur);
	ul_sched_enqueue(cur);

	next = sched_class->pick_next();
	if (!next || next == cur) {
		cur->state    = UL_THREAD_STATE_RUNNING;
		sched_current = cur;
		return;
	}

	/*
	 * Signal the ISR stub to perform the preemptive context switch.
	 */
	sched_current         = next;
	next->state           = UL_THREAD_STATE_RUNNING;
	next->ticks_remaining = UL_CONFIG_SCHED_QUANTUM_TICKS;
	ul_arch_mpu_switch(next->regions, next->region_count, 1u);
	g_preempt_old_ctx = &cur->ctx;
	g_preempt_new_ctx = &next->ctx;
}

/*
 * ul_sched_check_preempt — arm a preemptive switch from ISR context.
 *
 * Called after a generic ISR delivers a notification and may have woken
 * a higher-priority thread.  If the run-queue head has strictly higher
 * priority than the current thread, arms g_preempt_old/new_ctx so the
 * ISR stub (_arch_generic_preempt_isr) performs the switch on exit.
 *
 * The running thread stays in the run queue (pick_next does not remove it).
 * We only update its state to READY and update sched_current — no
 * ul_sched_enqueue() call is needed or correct here.
 */
void ul_sched_check_preempt(void)
{
	ul_thread_t *cur = sched_current;
	ul_thread_t *next;

	if (!cur || !sched_class)
		return;
	if (cur->state != UL_THREAD_STATE_RUNNING)
		return;

	next = sched_class->peek_next();
	if (!next || next == cur || next->priority >= cur->priority)
		return;

	cur->state = UL_THREAD_STATE_READY;

	sched_current         = next;
	next->state           = UL_THREAD_STATE_RUNNING;
	next->ticks_remaining = UL_CONFIG_SCHED_QUANTUM_TICKS;
	ul_arch_mpu_switch(next->regions, next->region_count, 1u);
	g_preempt_old_ctx = &cur->ctx;
	g_preempt_new_ctx = &next->ctx;
}
