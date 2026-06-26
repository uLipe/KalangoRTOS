/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Thread lifecycle handlers — kernel/thread/thread.c
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

/*
 * Thread registry: linear array for ul_thread_by_tid().
 * Populated by ul_thread_init(); never shrinks (dead threads stay registered
 * so their TID remains valid for diagnostic purposes).
 */
static ul_thread_t	*tcb_reg[UL_CONFIG_MAX_THREADS];
static uint32_t		 tcb_reg_count;
static ul_tid_t		 tid_counter;

int ul_thread_init(ul_thread_t *th, const ul_thread_attr_t *attr, void *stack)
{
	if (!th || !attr || !stack || !attr->entry)
		return UL_EINVAL;
	if (attr->stack_size == 0)
		return UL_EINVAL;
	if (tcb_reg_count >= UL_CONFIG_MAX_THREADS)
		return UL_ENOSPC;

	th->stack_base = (uint8_t *)stack;
	th->stack_size = attr->stack_size;
	th->priority   = attr->priority;
	th->state      = UL_THREAD_STATE_READY;
	th->privilege  = attr->privilege;
	th->tid        = tid_counter++;
	th->next       = NULL;

	ul_arch_ctx_init(&th->ctx,
			 attr->entry,
			 attr->arg,
			 (uintptr_t)stack + attr->stack_size,
			 attr->privilege);

	tcb_reg[tcb_reg_count++] = th;
	return UL_OK;
}

ul_thread_t *ul_thread_by_tid(ul_tid_t tid)
{
	uint32_t i;

	for (i = 0; i < tcb_reg_count; i++) {
		if (tcb_reg[i]->tid == tid)
			return tcb_reg[i];
	}
	return NULL;
}

void ul_thread_set_state(ul_thread_t *th, uint8_t state)
{
	th->state = state;
}

/* =========================================================================
 * Syscall handlers
 * ========================================================================= */

uint32_t ul_kern_thread_self(void)
{
	ul_thread_t *t = ul_sched_current();

	return t ? (uint32_t)t->tid : (uint32_t)(int32_t)UL_TID_INVALID;
}

/*
 * yield — re-enqueue current at the tail of its priority group (FIFO within
 * priority), then pick the highest-priority ready thread.
 */
uint32_t ul_kern_yield(void)
{
	ul_thread_t *cur = ul_sched_current();

	if (cur) {
		cur->state = UL_THREAD_STATE_READY;
		ul_sched_dequeue(cur);
		ul_sched_enqueue(cur);
	}
	ul_sched_schedule();
	return 0;
}

/*
 * exit — remove current thread from the run queue and schedule the next one.
 * Execution never returns to the exiting thread.
 */
uint32_t ul_kern_exit(void)
{
	ul_thread_t *cur = ul_sched_current();

	if (cur) {
		cur->state = UL_THREAD_STATE_DEAD;
		ul_sched_dequeue(cur);
	}
	ul_sched_schedule();
	for (;;)
		;
}

uint32_t ul_kern_thread_spawn(uint32_t attr_ptr)
{
	(void)attr_ptr;
	/* TODO: allocate stack from user_pool, initialise TCB, enqueue */
	return (uint32_t)(int32_t)UL_TID_INVALID;
}

uint32_t ul_kern_thread_kill(uint32_t tid)
{
	ul_thread_t *th = ul_thread_by_tid((ul_tid_t)tid);

	if (!th || th->state == UL_THREAD_STATE_DEAD)
		return (uint32_t)(int32_t)UL_ESRCH;

	th->state = UL_THREAD_STATE_DEAD;
	ul_sched_dequeue(th);

	if (th == ul_sched_current())
		ul_sched_schedule();

	return 0;
}

uint32_t ul_kern_thread_suspend(uint32_t tid)
{
	ul_thread_t *th = ul_thread_by_tid((ul_tid_t)tid);

	if (!th)
		return (uint32_t)(int32_t)UL_ESRCH;
	if (th->state == UL_THREAD_STATE_DEAD)
		return (uint32_t)(int32_t)UL_EINVAL;

	th->state = UL_THREAD_STATE_BLOCKED;
	ul_sched_dequeue(th);

	if (th == ul_sched_current())
		ul_sched_schedule();

	return 0;
}

uint32_t ul_kern_thread_resume(uint32_t tid)
{
	ul_thread_t *th = ul_thread_by_tid((ul_tid_t)tid);

	if (!th)
		return (uint32_t)(int32_t)UL_ESRCH;
	if (th->state != UL_THREAD_STATE_BLOCKED &&
	    th->state != UL_THREAD_STATE_SUSPENDED)
		return (uint32_t)(int32_t)UL_EINVAL;

	ul_sched_enqueue(th);
	return 0;
}

uint32_t ul_kern_thread_set_prio(uint32_t tid, uint32_t prio)
{
	ul_thread_t *th = ul_thread_by_tid((ul_tid_t)tid);

	if (!th)
		return (uint32_t)(int32_t)UL_ESRCH;

	if (th->state == UL_THREAD_STATE_READY) {
		/* Re-insert at new position in run queue. */
		ul_sched_dequeue(th);
		th->priority = (uint8_t)prio;
		ul_sched_enqueue(th);
	} else {
		/*
		 * RUNNING: just update priority; yield/suspend will use the
		 * new value when the thread re-enters the run queue.
		 * BLOCKED/SUSPENDED/DEAD: update for when thread is resumed.
		 */
		th->priority = (uint8_t)prio;
	}
	return 0;
}

uint32_t ul_kern_thread_get_prio(uint32_t tid)
{
	ul_thread_t *th = ul_thread_by_tid((ul_tid_t)tid);

	if (!th)
		return (uint32_t)(int32_t)UL_ESRCH;
	return (uint32_t)th->priority;
}
