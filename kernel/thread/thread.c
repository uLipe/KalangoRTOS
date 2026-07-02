/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Thread lifecycle handlers — kernel/thread/thread.c
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_timer_internal.h>
#include <kernel/include/ul_mem_internal.h>
#include <kernel/include/ul_ep_internal.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

/*
 * Thread registry: singly-linked list of ALL allocated TCBs (including
 * static idle/root threads).  ul_thread_by_tid() walks this list.
 * Protected by the invariant that TCBs are never moved.
 *
 * Static threads (idle, root) are registered at boot via ul_thread_init().
 * Dynamic threads append themselves at creation and remove themselves on exit.
 */
static ul_thread_t * UL_KERNEL_BSS tcb_list;
static ul_tid_t      UL_KERNEL_BSS tid_counter;

int ul_thread_init(ul_thread_t *th, const ul_thread_attr_t *attr, void *stack)
{
	ul_thread_t *p;

	if (!th || !attr || !stack || !attr->entry)
		return UL_EINVAL;
	if (attr->stack_size == 0)
		return UL_EINVAL;

	th->stack_base  = (uint8_t *)stack;
	th->stack_size  = attr->stack_size;
	th->priority    = attr->priority;
	th->saved_prio  = attr->priority;
	th->state       = UL_THREAD_STATE_READY;
	th->blocked_reason = UL_BLOCKED_NONE;
	th->privilege   = attr->privilege;
	th->tid         = tid_counter++;
	th->blocked_ep  = UL_EP_INVALID;
	th->blocked_notif = UL_NOTIF_INVALID;
	th->next        = NULL;
	th->sched_prev  = NULL;
	th->ipc_next    = NULL;
	th->ipc_sender  = UL_TID_INVALID;
	th->notif_received = 0u;
	th->notif_wait_mask = 0u;
	th->ipc_msg_outptr    = NULL;
	th->ipc_sender_outptr = NULL;
	th->notif_bits_outptr  = NULL;
	th->rn_result_outptr   = NULL;
	th->region_count      = 0u;
	th->ticks_remaining   = UL_CONFIG_SCHED_QUANTUM_TICKS;
	th->reg_next          = NULL;

	th->cap_flags = (attr->privilege == UL_PRIV_KERNEL) ? UL_CAP_ALL : 0u;

	/*
	 * Every non-kernel thread gets its stack as a default R+W MPU region.
	 */
	if (attr->privilege != UL_PRIV_KERNEL) {
		th->regions[0].base  = (uintptr_t)stack;
		th->regions[0].size  = attr->stack_size;
		th->regions[0].perms = UL_PERM_READ | UL_PERM_WRITE;
		th->regions[0].type  = UL_REGION_STACK;
		th->region_count     = 1u;
	}

	ul_arch_ctx_init(&th->ctx,
			 attr->entry,
			 attr->arg,
			 (uintptr_t)stack + attr->stack_size,
			 attr->privilege);

	/* Register in the TCB list (append at tail to preserve insertion order). */
	if (!tcb_list) {
		tcb_list = th;
	} else {
		p = tcb_list;
		while (p->reg_next)
			p = p->reg_next;
		p->reg_next = th;
	}

	return UL_OK;
}

ul_thread_t *ul_thread_by_tid(ul_tid_t tid)
{
	ul_thread_t *th;

	for (th = tcb_list; th; th = th->reg_next) {
		if (th->tid == tid)
			return th;
	}
	return NULL;
}

void ul_thread_set_state(ul_thread_t *th, uint8_t state)
{
	th->state = state;
}

/*
 * Unlink a TCB from the global registry list and free its heap memory.
 * Stack is freed first (it was allocated from the heap), then the TCB.
 * Called only for heap-allocated threads (dynamic TCBs).
 * Static TCBs (idle, root) must never be passed to this function.
 */
void ul_thread_free(ul_thread_t *th)
{
	ul_thread_t **pp;
	ul_arch_irq_key_t key;

	key = ul_arch_cpu_irq_save();

	/* Remove from registry list. */
	pp = &tcb_list;
	while (*pp && *pp != th)
		pp = &(*pp)->reg_next;
	if (*pp)
		*pp = th->reg_next;

	ul_arch_cpu_irq_restore(key);

	if (th->stack_base)
		ul_heap_free(th->stack_base);
	ul_heap_free(th);
}

/* =========================================================================
 * Syscall handlers
 * ========================================================================= */

uint32_t ul_kern_thread_self(void)
{
	ul_thread_t *t = ul_sched_current();

	return t ? (uint32_t)t->tid : (uint32_t)(int32_t)UL_TID_INVALID;
}

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
 * exit — mark thread dead and schedule the next one.
 *
 * Context chain and stack cannot be freed here: the thread is still
 * executing and its context chain is live.  Register for deferred cleanup.
 */
uint32_t ul_kern_exit(void)
{
	ul_thread_t *cur = ul_sched_current();

	if (cur) {
		cur->state = UL_THREAD_STATE_DEAD;
		ul_sched_dequeue(cur);
		ul_sched_set_dead_for_cleanup(cur);
	}
	ul_sched_schedule();
	for (;;)
		;
}

/*
 * spawn — allocate a TCB and stack from the heap, initialise, and enqueue.
 * Returns the new TID or a negative error code cast to uint32_t.
 */
uint32_t ul_kern_thread_spawn(uint32_t attr_ptr)
{
	const ul_thread_attr_t	*attr = (const ul_thread_attr_t *)(uintptr_t)attr_ptr;
	ul_thread_t		*th;
	void			*stack;
	int			 ret;

	if (!attr || !attr->entry || attr->stack_size == 0)
		return (uint32_t)(int32_t)UL_EINVAL;

	th = (ul_thread_t *)ul_heap_alloc(sizeof(ul_thread_t));
	if (!th)
		return (uint32_t)(int32_t)UL_ENOMEM;

	stack = ul_heap_alloc(attr->stack_size);
	if (!stack) {
		ul_heap_free(th);
		return (uint32_t)(int32_t)UL_ENOMEM;
	}

	ret = ul_thread_init(th, attr, stack);
	if (ret != UL_OK) {
		ul_heap_free(stack);
		ul_heap_free(th);
		return (uint32_t)(int32_t)ret;
	}

	ul_sched_enqueue(th);
	return (uint32_t)th->tid;
}

uint32_t ul_kern_thread_kill(uint32_t tid)
{
	ul_thread_t      *th = ul_thread_by_tid((ul_tid_t)tid);
	ul_arch_irq_key_t key;

	if (!th || th->state == UL_THREAD_STATE_DEAD)
		return (uint32_t)(int32_t)UL_ESRCH;

	/* Cancel timer wait if this thread is the current timer waiter. */
	if (th->blocked_reason == UL_BLOCKED_TIMER_WAIT)
		ul_timer_waiter_cancel(th);

	/* Remove from IPC recv_queue if the thread was waiting for a message. */
	if (th->blocked_reason == UL_BLOCKED_IPC_RECV ||
	    th->blocked_reason == UL_BLOCKED_IPC_OR_NOTIF)
		ul_ep_recv_queue_remove(th);

	th->state = UL_THREAD_STATE_DEAD;
	ul_sched_dequeue(th);

	if (th != ul_sched_current()) {
		/*
		 * Target is not on the CPU: context chain is not live, free now.
		 */
		key = ul_arch_cpu_irq_save();
		ul_arch_ctx_free(&th->ctx);
		ul_arch_cpu_irq_restore(key);
		ul_thread_free(th);
	} else {
		/* Self-kill: defer cleanup to reaper. */
		ul_sched_set_dead_for_cleanup(th);
		ul_sched_schedule();
	}

	return 0;
}

uint32_t ul_kern_thread_suspend(uint32_t tid)
{
	ul_thread_t *th = ul_thread_by_tid((ul_tid_t)tid);

	if (!th)
		return (uint32_t)(int32_t)UL_ESRCH;
	if (th->state == UL_THREAD_STATE_DEAD)
		return (uint32_t)(int32_t)UL_EINVAL;

	th->state = UL_THREAD_STATE_SUSPENDED;
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
	if (th->state != UL_THREAD_STATE_SUSPENDED)
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
		ul_sched_dequeue(th);
		th->priority = (uint8_t)prio;
		ul_sched_enqueue(th);
	} else {
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

uint32_t ul_kern_cap_grant(uint32_t target_tid, uint32_t caps)
{
	ul_thread_t *cur    = ul_sched_current();
	ul_thread_t *target = ul_thread_by_tid((ul_tid_t)target_tid);

	if (!cur || !target)
		return (uint32_t)(int32_t)UL_ESRCH;

	if ((cur->cap_flags & (uint8_t)caps) != (uint8_t)caps)
		return (uint32_t)(int32_t)UL_EPERM;

	target->cap_flags |= (uint8_t)caps;
	return (uint32_t)UL_OK;
}

uint32_t ul_kern_timer_set_deadline(uint32_t lo_us, uint32_t hi_us)
{
	uint64_t deadline_us = ((uint64_t)hi_us << 32) | (uint64_t)lo_us;

	if (deadline_us == 0u)
		return (uint32_t)(int32_t)UL_EINVAL;

	ul_timer_deadline_arm(deadline_us);
	return (uint32_t)UL_OK;
}

uint32_t ul_kern_timer_wait(void)
{
	ul_thread_t *cur = ul_sched_current();

	if (!cur)
		return (uint32_t)(int32_t)UL_EINVAL;

	ul_timer_wait_thread(cur);
	return (uint32_t)UL_OK;
}
