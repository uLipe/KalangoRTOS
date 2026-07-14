/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Scheduler core — kernel/sched/sched.c
 * Dispatches through ulmk_sched_class_t vtable; manages current/idle state.
 *
 * Context switches from syscall/IRQ hot paths happen only in
 * ulmk_sched_trap_dispatch() (deferred reschedule).
 *
 * IRQs are already masked for the whole kernel/ISR gateway on every arch;
 * this file does not nest irq_save/restore.
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
 * here; sched_switch_to() frees the context chain and stack at the start
 * of its next invocation, at which point the dead thread's context pointer
 * is saved in its ctx and is safe to walk.
 *
 * Only one slot is needed: a thread can exit at most once, and the next
 * switch always drains the slot before any new exit can enqueue another.
 */
static ulmk_thread_t * UL_KERNEL_BSS g_sched_dead;

static ulmk_arch_ctx_t UL_KERNEL_BSS startup_ctx;

/* Set by enqueue of higher-prio ready work or ulmk_sched_request_resched(). */
static bool UL_KERNEL_BSS needs_resched;

static inline __attribute__((always_inline))
uint8_t sched_thread_prs(const ulmk_thread_t *t)
{
	if (t && t->privilege == ULMK_PRIV_KERNEL)
		return 0u;
	return 1u;
}

static inline __attribute__((always_inline))
void sched_mark_preempt_if_higher(const ulmk_thread_t *t)
{
	if (sched_current && t != sched_current &&
	    t->priority < sched_current->priority)
		needs_resched = true;
}

static void sched_reap_dead(ulmk_thread_t *prev)
{
	/*
	 * The guard `g_sched_dead != prev` is critical: when a thread calls
	 * ulmk_kern_exit() it sets g_sched_dead = self and then immediately
	 * switches.  At that point the thread's context chain is still live —
	 * ulmk_arch_ctx_switch() is about to save it.  Freeing before that
	 * save would corrupt the CSA/stack frames.
	 */
	if (!g_sched_dead || g_sched_dead == prev)
		return;

	ulmk_arch_ctx_free(&g_sched_dead->ctx);
	ulmk_thread_free(g_sched_dead);
	g_sched_dead = NULL;
}

/*
 * Commit @next as current and switch from @prev's context.
 * Skips MPU reprogramming when the hardware context does not change.
 * Interrupt mask is left as the gateway left it (masked) — no irq_restore
 * that could enable IRQs mid-syscall after a resume.
 */
static void sched_switch_to(ulmk_thread_t *prev, ulmk_thread_t *next)
{
	ulmk_arch_ctx_t *from;
	ulmk_arch_ctx_t *to;

	from = prev ? &prev->ctx : &startup_ctx;
	to   = &next->ctx;

	if (from == to) {
		sched_current = next;
		next->state   = UL_THREAD_STATE_RUNNING;
		/*
		 * trap_dispatch may have enqueued @prev before pick_next();
		 * if the head did not change we must not leave a RUNNING
		 * thread linked in the ready queue.
		 */
		if (prev && sys_dnode_is_linked(&prev->sched_node)) {
			sched_class->dequeue(prev);
			prev->state = UL_THREAD_STATE_RUNNING;
		}
		return;
	}

	ulmk_arch_mpu_switch(next->regions, next->region_count,
			     sched_thread_prs(next));

	sched_current = next;
	next->state   = UL_THREAD_STATE_RUNNING;
	ulmk_arch_ctx_switch(from, to);
	/* Resumed here when THIS thread (prev) is re-scheduled. */
}

void ulmk_sched_set_dead_for_cleanup(ulmk_thread_t *th)
{
	if (g_sched_dead) {
		ulmk_arch_ctx_free(&g_sched_dead->ctx);
		ulmk_thread_free(g_sched_dead);
	}
	g_sched_dead = th;
}

void ulmk_sched_init(void)
{
	sched_current = NULL;
	sched_class   = NULL;
	needs_resched = false;
}

void ulmk_sched_set_class(const ulmk_sched_class_t *cls)
{
	sched_class = cls;
	cls->init();
}

void ulmk_sched_start(void)
{
	ulmk_thread_t *first = sched_class->pick_next();

	sched_current          = first;
	first->state           = UL_THREAD_STATE_RUNNING;
	needs_resched          = false;
	ulmk_arch_mpu_switch(first->regions, first->region_count,
			     sched_thread_prs(first));
	ulmk_arch_ctx_switch(&startup_ctx, &first->ctx);
}

void ulmk_sched_request_resched(void)
{
	needs_resched = true;
}

void ulmk_sched_resched(void)
{
	ulmk_thread_t *prev = sched_current;
	ulmk_thread_t *next;

	needs_resched = false;
	sched_reap_dead(prev);
	next = sched_class->pick_next();
	sched_switch_to(prev, next);
}

void ulmk_sched_enqueue(ulmk_thread_t *t)
{
	sched_class->enqueue(t);
	sched_mark_preempt_if_higher(t);
}

void ulmk_sched_dequeue(ulmk_thread_t *t)
{
	sched_class->dequeue(t);
}

/*
 * Locked aliases — same as unlocked under the kernel-masked-IRQ contract.
 * Kept so existing callers (notif critical sections) stay readable.
 */
void ulmk_sched_enqueue_locked(ulmk_thread_t *t)
{
	ulmk_sched_enqueue(t);
}

void ulmk_sched_dequeue_locked(ulmk_thread_t *t)
{
	ulmk_sched_dequeue(t);
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
 *
 * If the current thread is no longer RUNNING (blocked/dead/ready-requeued by
 * a syscall that deferred the switch), pick the next thread immediately.
 * Otherwise apply classic priority preemption / requested resched.
 */
void ulmk_sched_trap_dispatch(bool from_isr)
{
	ulmk_thread_t *cur;
	ulmk_thread_t *next;
	bool           want;

	cur = sched_current;

	if (!cur || !sched_class)
		return;

	want = needs_resched;
	needs_resched = false;

	if (cur->state != UL_THREAD_STATE_RUNNING) {
		sched_reap_dead(cur);
		next = sched_class->pick_next();
		sched_switch_to(cur, next);
		return;
	}

	if (!want) {
		next = sched_class->peek_next();
		if (!next || next == cur || next->priority >= cur->priority)
			return;
	}

	if (from_isr) {
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

	if (sys_dnode_is_linked(&cur->sched_node))
		sched_class->dequeue(cur);
	cur->state = UL_THREAD_STATE_READY;
	sched_class->enqueue(cur);

	next = sched_class->pick_next();
	sched_switch_to(cur, next);
}
