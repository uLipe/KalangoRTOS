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
 * Thread registry: linear array for ul_thread_by_tid().
 * Populated by ul_thread_init(); never shrinks (dead threads stay registered
 * so their TID remains valid for diagnostic purposes).
 */
static ul_thread_t	*tcb_reg[UL_CONFIG_MAX_THREADS];
static uint32_t		 tcb_reg_count;
static ul_tid_t		 tid_counter;

/*
 * TCB pool for dynamically-created threads (ul_kern_thread_spawn).
 * Static threads created via ul_thread_init() use caller-provided storage
 * and are NOT in this pool.
 */
static ul_thread_t	 tcb_pool[UL_CONFIG_MAX_THREADS];
static bool		 tcb_pool_used[UL_CONFIG_MAX_THREADS];

int ul_thread_init(ul_thread_t *th, const ul_thread_attr_t *attr, void *stack)
{
	if (!th || !attr || !stack || !attr->entry)
		return UL_EINVAL;
	if (attr->stack_size == 0)
		return UL_EINVAL;
	if (tcb_reg_count >= UL_CONFIG_MAX_THREADS)
		return UL_ENOSPC;

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
	th->sleep_next  = NULL;
	th->ipc_next    = NULL;
	th->sleep_until = 0u;
	th->ipc_sender  = UL_TID_INVALID;
	th->notif_received = 0u;
	th->notif_wait_mask = 0u;
	th->ipc_msg_outptr    = NULL;
	th->ipc_sender_outptr = NULL;
	th->notif_bits_outptr  = NULL;
	th->rn_result_outptr   = NULL;
	th->region_count      = 0u;

	/*
	 * Default capability set:
	 *   Kernel threads (root) receive UL_CAP_ALL.
	 *   Driver/user threads spawned dynamically start with no capabilities;
	 *   root must explicitly grant via ul_cap_grant().
	 */
	th->cap_flags = (attr->privilege == UL_PRIV_KERNEL) ? UL_CAP_ALL : 0u;

	/*
	 * Every non-kernel thread gets its stack as a default R+W MPU region.
	 * The kernel thread uses PRS 0 (full access) and needs no DPR entries.
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
 * Internal helpers
 * ========================================================================= */

/*
 * Free a pool-allocated TCB slot.  No-op for statically-allocated threads.
 */
static void tcb_pool_free(ul_thread_t *th)
{
	uint32_t i;

	for (i = 0; i < UL_CONFIG_MAX_THREADS; i++) {
		if (&tcb_pool[i] == th) {
			tcb_pool_used[i] = false;
			return;
		}
	}
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
		tcb_pool_free(cur);
	}
	ul_sched_schedule();
	for (;;)
		;
}

/*
 * spawn — allocate a TCB from the pool, allocate a stack from the phys pool,
 * initialise the thread, and enqueue it.  Returns the new TID or a negative
 * error code cast to uint32_t.
 */
uint32_t ul_kern_thread_spawn(uint32_t attr_ptr)
{
	const ul_thread_attr_t	*attr = (const ul_thread_attr_t *)(uintptr_t)attr_ptr;
	ul_thread_t		*th = NULL;
	void			*stack;
	uint32_t		 slot;
	int			 ret;

	if (!attr || !attr->entry || attr->stack_size == 0)
		return (uint32_t)(int32_t)UL_EINVAL;

	for (slot = 0; slot < UL_CONFIG_MAX_THREADS; slot++) {
		if (!tcb_pool_used[slot]) {
			th = &tcb_pool[slot];
			tcb_pool_used[slot] = true;
			break;
		}
	}
	if (!th)
		return (uint32_t)(int32_t)UL_ENOSPC;

	stack = ul_phys_alloc(attr->stack_size);
	if (!stack) {
		tcb_pool_used[slot] = false;
		return (uint32_t)(int32_t)UL_ENOMEM;
	}

	ret = ul_thread_init(th, attr, stack);
	if (ret != UL_OK) {
		tcb_pool_used[slot] = false;
		return (uint32_t)(int32_t)ret;
	}

	ul_sched_enqueue(th);
	return (uint32_t)th->tid;
}

uint32_t ul_kern_thread_kill(uint32_t tid)
{
	ul_thread_t *th = ul_thread_by_tid((ul_tid_t)tid);

	if (!th || th->state == UL_THREAD_STATE_DEAD)
		return (uint32_t)(int32_t)UL_ESRCH;

	/* Cancel any pending sleep. */
	if (th->sleep_until != 0u)
		ul_timer_sleep_remove(th);

	/* Remove from IPC recv_queue if the thread was waiting for a message. */
	if (th->blocked_reason == UL_BLOCKED_IPC_RECV ||
	    th->blocked_reason == UL_BLOCKED_IPC_OR_NOTIF)
		ul_ep_recv_queue_remove(th);

	th->state = UL_THREAD_STATE_DEAD;
	ul_sched_dequeue(th);
	tcb_pool_free(th);

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
		/*
		 * RUNNING, BLOCKED, SUSPENDED, DEAD: update priority for when
		 * the thread re-enters the run queue.
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

/*
 * ul_kern_cap_grant — grant a subset of capabilities to another thread.
 *
 * Caller must hold UL_CAP_GRANT_CAP and all of the caps being granted.
 * Prevents privilege escalation: a thread cannot grant caps it does not hold.
 */
uint32_t ul_kern_cap_grant(uint32_t target_tid, uint32_t caps)
{
	ul_thread_t *cur    = ul_sched_current();
	ul_thread_t *target = ul_thread_by_tid((ul_tid_t)target_tid);

	if (!cur || !target)
		return (uint32_t)(int32_t)UL_ESRCH;

	/* Cannot grant capabilities the caller does not hold */
	if ((cur->cap_flags & (uint8_t)caps) != (uint8_t)caps)
		return (uint32_t)(int32_t)UL_EPERM;

	target->cap_flags |= (uint8_t)caps;
	return (uint32_t)UL_OK;
}

/*
 * ul_kern_sleep_us — block the calling thread for @lo_us | (@hi_us << 32) µs.
 *
 * The 64-bit duration is split across two 32-bit syscall argument registers
 * (D4 = lo, D5 = hi) and reconstructed here.  A zero duration is treated as
 * an immediate yield.  The thread is inserted into the sleep queue and
 * ul_sched_schedule() switches to the next ready thread; execution resumes
 * here after the timer ISR calls ul_sched_enqueue() for this thread.
 */
uint32_t ul_kern_sleep_us(uint32_t lo_us, uint32_t hi_us)
{
	uint64_t	 duration = ((uint64_t)hi_us << 32) | (uint64_t)lo_us;
	uint64_t	 deadline;
	ul_thread_t	*cur = ul_sched_current();

	if (!cur)
		return (uint32_t)(int32_t)UL_EINVAL;

	if (duration == 0u) {
		ul_kern_yield();
		return 0;
	}

	deadline   = ul_timer_now_us() + duration;
	cur->state          = UL_THREAD_STATE_BLOCKED;
	cur->blocked_reason = UL_BLOCKED_SLEEP;
	ul_sched_dequeue(cur);
	ul_timer_sleep_insert(cur, deadline);
	ul_sched_schedule();

	return 0;
}
