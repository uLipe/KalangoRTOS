/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * IPC endpoint handlers — kernel/ipc/ep.c
 * Reference: docs/api_spec.md §7
 *
 * Rendezvous model (seL4-inspired):
 *   - ep_call: caller blocks; if server waiting, wake server immediately.
 *   - ep_recv: server blocks; if caller waiting, deliver immediately.
 *   - ep_reply: wake the blocked caller; server keeps running (cooperative).
 *   - ep_reply_recv: atomic reply + re-block for next message (fast path).
 *
 * Priority inheritance:
 *   When ep_call wakes a server, the server's priority is temporarily raised
 *   to the caller's priority if the caller is higher-priority.
 *   ep_reply restores the server's original priority before waking the caller.
 *
 * Two-layer design:
 *   ep_*_impl() — core logic with native C pointer types; testable on host.
 *   ulmk_kern_ep_*() — syscall ABI wrappers; cast uint32_t args and delegate.
 *
 * Capability enforcement: deferred — any valid endpoint ID may be used.
 */

#include <stdint.h>
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_ep_internal.h>
#include <kernel/include/ulmk_notif_internal.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/syscall/syscall_router.h>
#include <ulmk_arch.h>

#ifndef UL_UNIT_TEST
#include <kernel/include/ulmk_mem_internal.h>
#else
#include <ulmk/config.h>
#endif

#ifdef UL_UNIT_TEST
ulmk_endpoint_t ep_pool[ULMK_CONFIG_MAX_ENDPOINTS];
#endif

int ulmk_ep_init(ulmk_endpoint_t *ep, ulmk_ep_t id)
{
	ep->id     = id;
	ep->active = true;
	sys_dlist_init(&ep->send_queue);
	sys_dlist_init(&ep->recv_queue);
	return 0;
}

ulmk_endpoint_t *ulmk_ep_by_id(ulmk_ep_t id)
{
#ifdef UL_UNIT_TEST
	/*
	 * Unit test mode: integer IDs index into the static pool.
	 * ep_pool is accessible to tests via extern.
	 */
	if (id == ULMK_EP_INVALID || (uint32_t)id >= ULMK_CONFIG_MAX_ENDPOINTS)
		return NULL;
	if (!ep_pool[(uint32_t)id].active)
		return NULL;
	return &ep_pool[(uint32_t)id];
#else
	ulmk_endpoint_t *ep = (ulmk_endpoint_t *)(uintptr_t)id;

	if (!ep || !ep->active)
		return NULL;
	return ep;
#endif
}

void ulmk_ep_recv_queue_remove(ulmk_thread_t *th)
{
	if (th->blocked_ep == ULMK_EP_INVALID)
		return;

	if (sys_dnode_is_linked(&th->ipc_node)) {
		sys_dlist_remove(&th->ipc_node);
		sys_dnode_init(&th->ipc_node);
	}
	th->blocked_ep = ULMK_EP_INVALID;
}

/* =========================================================================
 * Helpers
 * ========================================================================= */

static ulmk_thread_t *ipc_pop_head(sys_dlist_t *queue)
{
	sys_dnode_t     *dn;
	ulmk_thread_t   *th;

	dn = sys_dlist_get(queue);
	if (!dn)
		return NULL;

	th = SYS_DLIST_CONTAINER_OF(dn, ulmk_thread_t, ipc_node);
	sys_dnode_init(&th->ipc_node);
	return th;
}

static void ipc_enqueue_tail(sys_dlist_t *queue, ulmk_thread_t *th)
{
	sys_dlist_append(queue, &th->ipc_node);
}

static void deliver_to_server(ulmk_thread_t *server, ulmk_thread_t *caller,
			      const ulmk_msg_t *msg)
{
	server->ipc_msg    = *msg;
	server->ipc_sender = caller->tid;

	server->saved_prio = server->priority;
	if (caller->priority < server->priority)
		server->priority = caller->priority;

	server->state          = UL_THREAD_STATE_READY;
	server->blocked_reason = UL_BLOCKED_NONE;
	server->blocked_ep     = ULMK_EP_INVALID;

	if (server->blocked_notif != ULMK_NOTIF_INVALID) {
		ulmk_notif_obj_t *n = ulmk_notif_by_id(server->blocked_notif);

		if (n && n->waiter == server)
			n->waiter = NULL;
		server->blocked_notif = ULMK_NOTIF_INVALID;
	}

	ulmk_sched_enqueue(server);
}

/* =========================================================================
 * Core implementation (_impl functions — native pointer types)
 * ========================================================================= */

int ep_call_impl(ulmk_ep_t ep_id, ulmk_msg_t *msg)
{
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *cur;

	if (!msg)
		return -ULMK_EINVAL;

	ep = ulmk_ep_by_id(ep_id);
	if (!ep)
		return -ULMK_EINVAL;

	cur = ulmk_sched_current();
	if (!cur)
		return -ULMK_EINVAL;

	cur->ipc_msg           = *msg;
	cur->ipc_sender        = ULMK_TID_INVALID;
	cur->blocked_reason    = UL_BLOCKED_IPC_CALL;
	cur->blocked_ep        = ep_id;
	cur->ipc_msg_outptr    = msg;

	if (!sys_dlist_is_empty(&ep->recv_queue)) {
		ulmk_thread_t *srv = ipc_pop_head(&ep->recv_queue);

		deliver_to_server(srv, cur, &cur->ipc_msg);
	} else {
		ipc_enqueue_tail(&ep->send_queue, cur);
	}

	cur->state = UL_THREAD_STATE_BLOCKED;
	ulmk_sched_dequeue(cur);
	ulmk_sched_resched();

	/* Re-fetch cur: local var may be stale after context switch. */
	cur = ulmk_sched_current();
	if (cur && cur->ipc_msg_outptr) {
		*cur->ipc_msg_outptr = cur->ipc_msg;
		cur->ipc_msg_outptr  = NULL;
	}
	return 0;
}

int ep_recv_impl(ulmk_ep_t ep_id, ulmk_msg_t *msg, ulmk_tid_t *sender)
{
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *cur;

	if (!msg)
		return -ULMK_EINVAL;

	ep = ulmk_ep_by_id(ep_id);
	if (!ep)
		return -ULMK_EINVAL;

	cur = ulmk_sched_current();
	if (!cur)
		return -ULMK_EINVAL;

	if (!sys_dlist_is_empty(&ep->send_queue)) {
		ulmk_thread_t *caller = ipc_pop_head(&ep->send_queue);

		*msg = caller->ipc_msg;
		cur->ipc_sender = caller->tid;

		if (sender)
			*sender = caller->tid;

		return 0;
	}

	cur->ipc_msg_outptr    = msg;
	cur->ipc_sender_outptr = sender;
	cur->blocked_reason    = UL_BLOCKED_IPC_RECV;
	cur->blocked_ep        = ep_id;
	ipc_enqueue_tail(&ep->recv_queue, cur);

	cur->state = UL_THREAD_STATE_BLOCKED;
	ulmk_sched_dequeue(cur);
	ulmk_sched_resched();

	cur = ulmk_sched_current();
	if (!cur)
		return -ULMK_EINVAL;

	if (cur->ipc_msg_outptr) {
		*cur->ipc_msg_outptr = cur->ipc_msg;
		cur->ipc_msg_outptr  = NULL;
	}
	if (cur->ipc_sender_outptr) {
		*cur->ipc_sender_outptr = cur->ipc_sender;
		cur->ipc_sender_outptr  = NULL;
	}
	return 0;
}

int ep_reply_impl(ulmk_tid_t sender_tid, const ulmk_msg_t *reply)
{
	ulmk_thread_t *cur    = ulmk_sched_current();
	ulmk_thread_t *caller;

	if (!reply)
		return -ULMK_EINVAL;

	caller = ulmk_thread_by_tid(sender_tid);
	if (!caller || caller->blocked_reason != UL_BLOCKED_IPC_CALL)
		return -ULMK_EINVAL;

	if (cur)
		cur->priority = cur->saved_prio;

	caller->ipc_msg        = *reply;
	caller->state          = UL_THREAD_STATE_READY;
	caller->blocked_reason = UL_BLOCKED_NONE;
	caller->blocked_ep     = ULMK_EP_INVALID;
	ulmk_sched_enqueue(caller);

	return 0;
}

int ep_reply_recv_impl(ulmk_ep_t ep_id, ulmk_tid_t sender_tid,
		       const ulmk_msg_t *reply, ulmk_msg_t *next,
		       ulmk_tid_t *next_sender)
{
	if (!next)
		return -ULMK_EINVAL;

	if (sender_tid != ULMK_TID_INVALID)
		ep_reply_impl(sender_tid, reply);

	return ep_recv_impl(ep_id, next, next_sender);
}

int ep_grant_impl(ulmk_ep_t ep_id, ulmk_tid_t target_tid)
{
	if (!ulmk_ep_by_id(ep_id))
		return -ULMK_EINVAL;
	if (!ulmk_thread_by_tid(target_tid))
		return -ULMK_EINVAL;
	return 0;
}

int ep_recv_or_notif_impl(ulmk_ep_t ep_id, ulmk_notif_t notif_id,
			  uint32_t mask, ulmk_recv_or_notif_result_t *res)
{
	ulmk_endpoint_t  *ep;
	ulmk_notif_obj_t *n;
	ulmk_thread_t    *cur;
	uint32_t        matched;

	if (!res)
		return -ULMK_EINVAL;

	ep = ulmk_ep_by_id(ep_id);
	if (!ep)
		return -ULMK_EINVAL;

	n = ulmk_notif_by_id(notif_id);
	if (!n)
		return -ULMK_EINVAL;

	cur = ulmk_sched_current();
	if (!cur)
		return -ULMK_EINVAL;

	matched = n->bits & mask;
	if (matched) {
		n->bits         &= ~matched;
		res->is_notif    = 1;
		res->notif_bits  = matched;
		res->sender      = ULMK_TID_INVALID;
		return 1;
	}

	if (!sys_dlist_is_empty(&ep->send_queue)) {
		ulmk_thread_t *caller = ipc_pop_head(&ep->send_queue);

		cur->ipc_sender = caller->tid;
		cur->ipc_msg    = caller->ipc_msg;

		res->is_notif = 0;
		res->msg      = cur->ipc_msg;
		res->sender   = cur->ipc_sender;
		return 0;
	}

	cur->ipc_sender        = ULMK_TID_INVALID;
	cur->blocked_reason    = UL_BLOCKED_IPC_OR_NOTIF;
	cur->blocked_ep        = ep_id;
	cur->blocked_notif     = notif_id;
	cur->notif_wait_mask   = mask;
	cur->ipc_msg_outptr    = NULL;
	cur->ipc_sender_outptr = NULL;
	cur->rn_result_outptr  = res;

	ipc_enqueue_tail(&ep->recv_queue, cur);
	n->waiter    = cur;
	n->wait_mask = mask;

	cur->state = UL_THREAD_STATE_BLOCKED;
	ulmk_sched_dequeue(cur);
	ulmk_sched_resched();

	/* Re-fetch cur: local var may be stale after context switch. */
	cur = ulmk_sched_current();
	if (!cur)
		return -ULMK_EINVAL;

	res = cur->rn_result_outptr;
	cur->rn_result_outptr = NULL;

	if (!res)
		return -ULMK_EINVAL;

	if (cur->ipc_sender != ULMK_TID_INVALID) {
		res->is_notif   = 0;
		res->msg        = cur->ipc_msg;
		res->sender     = cur->ipc_sender;
		res->notif_bits = 0;
		return 0;
	} else {
		res->is_notif   = 1;
		res->notif_bits = cur->notif_received;
		res->sender     = ULMK_TID_INVALID;
		return 1;
	}
}

/* =========================================================================
 * Syscall ABI wrappers — cast uint32_t args, delegate to _impl.
 * ========================================================================= */

uint32_t ulmk_kern_ep_create(void)
{
#ifdef UL_UNIT_TEST
	uint32_t i;

	for (i = 0u; i < ULMK_CONFIG_MAX_ENDPOINTS; i++) {
		if (!ep_pool[i].active) {
			ulmk_ep_init(&ep_pool[i], (ulmk_ep_t)i);
			return (uint32_t)i;
		}
	}
	return (uint32_t)(int32_t)(-ULMK_ENOSPC);
#else
	ulmk_endpoint_t *ep = (ulmk_endpoint_t *)ulmk_heap_alloc(sizeof(ulmk_endpoint_t));

	if (!ep)
		return (uint32_t)(int32_t)(-ULMK_ENOMEM);
	ulmk_ep_init(ep, (ulmk_ep_t)(uintptr_t)ep);
	return (uint32_t)(uintptr_t)ep;
#endif
}

uint32_t ulmk_kern_ep_call(uint32_t ep_id, uint32_t msg_ptr)
{
	return (uint32_t)(int32_t)ep_call_impl(
		(ulmk_ep_t)ep_id,
		(ulmk_msg_t *)(uintptr_t)msg_ptr);
}

uint32_t ulmk_kern_ep_recv(uint32_t ep_id, uint32_t msg_ptr,
			 uint32_t sender_ptr)
{
	return (uint32_t)(int32_t)ep_recv_impl(
		(ulmk_ep_t)ep_id,
		(ulmk_msg_t  *)(uintptr_t)msg_ptr,
		(ulmk_tid_t  *)(uintptr_t)sender_ptr);
}

uint32_t ulmk_kern_ep_reply(uint32_t sender_tid_u32, uint32_t reply_ptr)
{
	return (uint32_t)(int32_t)ep_reply_impl(
		(ulmk_tid_t)sender_tid_u32,
		(const ulmk_msg_t *)(uintptr_t)reply_ptr);
}

uint32_t ulmk_kern_ep_reply_recv(uint32_t ep_id, uint32_t sender_tid_u32,
			       uint32_t args_ptr)
{
	const ulmk_reply_recv_args_t *args =
		(const ulmk_reply_recv_args_t *)(uintptr_t)args_ptr;

	if (!args)
		return (uint32_t)(int32_t)(-ULMK_EINVAL);

	return (uint32_t)(int32_t)ep_reply_recv_impl(
		(ulmk_ep_t)ep_id,
		(ulmk_tid_t)sender_tid_u32,
		args->reply, args->next, args->next_sender);
}

uint32_t ulmk_kern_ep_grant(uint32_t ep_id, uint32_t target_tid)
{
	return (uint32_t)(int32_t)ep_grant_impl(
		(ulmk_ep_t)ep_id, (ulmk_tid_t)target_tid);
}

uint32_t ulmk_kern_ep_recv_or_notif(uint32_t ep_id, uint32_t notif_id,
				  uint32_t mask, uint32_t result_ptr)
{
	return (uint32_t)(int32_t)ep_recv_or_notif_impl(
		(ulmk_ep_t)ep_id,
		(ulmk_notif_t)notif_id,
		mask,
		(ulmk_recv_or_notif_result_t *)(uintptr_t)result_ptr);
}

uint32_t ulmk_kern_ep_destroy(uint32_t ep_id)
{
#ifdef UL_UNIT_TEST
	uint32_t i = (uint32_t)ep_id;

	if (i >= ULMK_CONFIG_MAX_ENDPOINTS || !ep_pool[i].active)
		return (uint32_t)(int32_t)(-ULMK_EINVAL);
	ep_pool[i].active = false;
	return 0u;
#else
	ulmk_endpoint_t *ep = (ulmk_endpoint_t *)(uintptr_t)ep_id;

	if (!ep || !ep->active)
		return (uint32_t)(int32_t)(-ULMK_EINVAL);
	ep->active = false;
	ulmk_heap_free(ep);
	return 0u;
#endif
}
