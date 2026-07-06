/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Scheduler core — kernel/sched/sched.c
 * Dispatches through ulmk_sched_class_t vtable; manages current/idle state.
 */

#include <stddef.h>
#include <stdbool.h>
#include <ulmk/config.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/include/ulmk_thread_internal.h>
#include <kernel/include/ulmk_mem_internal.h>
#include <ulmk_arch.h>

static const ulmk_sched_class_t * UL_KERNEL_BSS sched_class;
static ulmk_thread_t            * UL_KERNEL_BSS sched_current;

/*
 * Dead-thread reaper slot.
 *
 * When a thread calls ulmk_thread_exit() it is still on the CPU — its
 * hardware context chain is live in the arch context register.  We cannot
 * walk and free that chain until after the context switch hands control to
 * the next thread.  ulmk_sched_set_dead_for_cleanup() stores the dying thread
 * here; ulmk_sched_resched() frees the context chain and stack at the start
 * of its next invocation, at which point the dead thread's context pointer
 * is saved in its ctx and is safe to walk.
 *
 * Only one slot is needed: a thread can exit at most once, and the next
 * call to ulmk_sched_resched() always drains the slot before any new exit
 * can enqueue another dying thread.
 */
static ulmk_thread_t * UL_KERNEL_BSS g_sched_dead;

static uint8_t sched_thread_prs(const ulmk_thread_t *t)
{
	if (t && t->privilege == ULMK_PRIV_KERNEL)
		return 0u;
	return 1u;
}

static ulmk_arch_ctx_t UL_KERNEL_BSS startup_ctx;

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
 * Called with interrupts enabled; ulmk_arch_ctx_free() is guarded
 * internally by a local irq_save/restore.
 */
void ulmk_sched_set_dead_for_cleanup(ulmk_thread_t *th)
{
	ulmk_arch_irq_key_t key;

	key = ulmk_arch_cpu_irq_save();
	if (g_sched_dead) {
		ulmk_arch_ctx_free(&g_sched_dead->ctx);
		ulmk_thread_free(g_sched_dead);
	}
	g_sched_dead = th;
	ulmk_arch_cpu_irq_restore(key);
}

void ulmk_sched_init(void)
{
	sched_current = NULL;
	sched_class   = NULL;
}

void ulmk_sched_set_class(const ulmk_sched_class_t *cls)
{
	sched_class = cls;
	cls->init();
}

/*
 * ulmk_sched_start — perform the first context switch from the startup frame
 * to the highest-priority ready thread.
 *
 * The startup frame context (startup_ctx) is written once and never loaded
 * again; it acts purely as the "from" argument for the initial switch.
 * Since the idle thread is always in the run queue, pick_next() is
 * guaranteed to return a valid thread.  Does not return.
 */
void ulmk_sched_start(void)
{
	ulmk_thread_t *first = sched_class->pick_next();

	sched_current          = first;
	first->state           = UL_THREAD_STATE_RUNNING;
	ulmk_arch_mpu_switch(first->regions, first->region_count,
			     sched_thread_prs(first));
	ulmk_arch_ctx_switch(&startup_ctx, &first->ctx);
}

/*
 * ulmk_sched_resched — switch to the highest-priority ready thread.
 *
 * The running thread must have already set its own state to BLOCKED/DEAD
 * (and dequeued itself) before calling, OR set it to READY (remaining in
 * the queue) if voluntarily yielding the CPU.
 *
 * With the idle thread permanently in the queue, pick_next() always
 * returns a valid thread, so there is no idle-context fallback path.
 */
void ulmk_sched_resched(void)
{
	ulmk_thread_t      *prev = sched_current;
	ulmk_thread_t      *next;
	ulmk_arch_ctx_t    *from;
	ulmk_arch_ctx_t    *to;
	ulmk_arch_irq_key_t key;

	/*
	 * Release context chain and stack of a previously-dead thread.
	 *
	 * The guard `g_sched_dead != prev` is critical: when a thread calls
	 * ulmk_kern_exit() it sets g_sched_dead = self and then immediately
	 * calls ulmk_sched_resched().  At that point the thread's context chain
	 * is still live — ulmk_arch_ctx_switch() is about to save the current
	 * lower context into a free frame and record the resulting context
	 * pointer into g_sched_dead->ctx.  Freeing the chain before that save
	 * would return those frames to the free list, only for the save
	 * instruction to immediately re-allocate and corrupt them.
	 */
	if (g_sched_dead && g_sched_dead != prev) {
		key = ulmk_arch_cpu_irq_save();
		ulmk_arch_ctx_free(&g_sched_dead->ctx);
		ulmk_thread_free(g_sched_dead);
		g_sched_dead = NULL;
		ulmk_arch_cpu_irq_restore(key);
	}

	next = sched_class->pick_next();
	from = prev ? &prev->ctx : &startup_ctx;

	/*
	 * Disable interrupts before committing sched_current = next.
	 */
	key = ulmk_arch_cpu_irq_save();

	sched_current          = next;
	next->state            = UL_THREAD_STATE_RUNNING;
	to = &next->ctx;
	ulmk_arch_mpu_switch(next->regions, next->region_count,
			     sched_thread_prs(next));

	if (from == to) {
		/*
		 * trap_dispatch enqueued @prev before pick_next(); if the head
		 * did not change we must not leave a RUNNING thread linked.
		 */
		if (prev && sys_dnode_is_linked(&prev->sched_node)) {
			sched_class->dequeue(prev);
			prev->state = UL_THREAD_STATE_RUNNING;
		}
		ulmk_arch_cpu_irq_enable();
		return;
	}

	ulmk_arch_ctx_switch(from, to);
	/*
	 * We return here when THIS thread (prev) is re-scheduled.
	 *
	 * The arch context restore does not re-enable interrupts (IE was
	 * cleared by irq_save() before the switch).  Re-enable unconditionally.
	 */
	ulmk_arch_cpu_irq_enable();
}

void ulmk_sched_enqueue(ulmk_thread_t *t)
{
	ulmk_arch_irq_key_t key;

	key = ulmk_arch_cpu_irq_save();
	sched_class->enqueue(t);
	ulmk_arch_cpu_irq_restore(key);
}

void ulmk_sched_dequeue(ulmk_thread_t *t)
{
	ulmk_arch_irq_key_t key;

	key = ulmk_arch_cpu_irq_save();
	sched_class->dequeue(t);
	ulmk_arch_cpu_irq_restore(key);
}

ulmk_thread_t *ulmk_sched_current(void)
{
	return sched_current;
}

ulmk_thread_t *ulmk_sched_peek_next(void)
{
	return sched_class ? sched_class->peek_next() : NULL;
}

/*
 * ulmk_sched_trap_dispatch — trap/ISR exit scheduling.
 */
void ulmk_sched_trap_dispatch(bool from_isr)
{
	ulmk_thread_t *cur;
	ulmk_thread_t *next;

	if (from_isr) {
		cur = sched_current;

		if (!cur || !sched_class)
			return;
		if (cur->state != UL_THREAD_STATE_RUNNING)
			return;

		next = sched_class->peek_next();
		if (!next || next == cur || next->priority >= cur->priority)
			return;

		cur->state = UL_THREAD_STATE_READY;
		if (sys_dnode_is_linked(&cur->sched_node))
			sched_class->dequeue(cur);
		sched_class->enqueue(cur);

		if (ulmk_arch_sched_isr_preempt_deferred()) {
			next = sched_class->pick_next();

			sched_current = next;
			next->state   = UL_THREAD_STATE_RUNNING;
			ulmk_arch_mpu_switch(next->regions, next->region_count,
					     sched_thread_prs(next));
			ulmk_arch_sched_switch(&cur->ctx, &next->ctx,
					       ULMK_SCHED_SWITCH_PREEMPT_ISR);
			return;
		}

		ulmk_sched_resched();
		return;
	}

	cur = sched_current;
	if (!cur || !sched_class)
		return;
	if (cur->state != UL_THREAD_STATE_RUNNING)
		return;

	next = sched_class->peek_next();
	if (!next || next == cur || next->priority >= cur->priority)
		return;

	if (sys_dnode_is_linked(&cur->sched_node))
		sched_class->dequeue(cur);
	cur->state = UL_THREAD_STATE_READY;
	sched_class->enqueue(cur);

	next = sched_class->pick_next();

	sched_current = next;
	next->state   = UL_THREAD_STATE_RUNNING;
	ulmk_arch_mpu_switch(next->regions, next->region_count,
			     sched_thread_prs(next));
	ulmk_arch_sched_switch(&cur->ctx, &next->ctx, 0);
}
