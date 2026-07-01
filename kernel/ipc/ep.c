/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * IPC endpoint handlers — kernel/ipc/ep.c
 * Reference: docs/api_spec.md §7, docs/microkernel_book_tricore.md §9
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
 *   ul_kern_ep_*() — syscall ABI wrappers; cast uint32_t args and delegate.
 *
 * Capability enforcement: deferred — any valid endpoint ID may be used.
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_ep_internal.h>
#include <kernel/include/ul_notif_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

#ifndef UL_UNIT_TEST
#include <kernel/include/ul_mem_internal.h>
#endif

int ul_ep_init(ul_endpoint_t *ep, ul_ep_t id)
{
	ep->id         = id;
	ep->active     = true;
	ep->send_queue = NULL;
	ep->recv_queue = NULL;
	return 0;
}

ul_endpoint_t *ul_ep_by_id(ul_ep_t id)
{
#ifdef UL_UNIT_TEST
	/*
	 * Unit test mode: integer IDs index into the static pool.
	 * ep_pool is defined below and accessible to tests via extern.
	 */
	if (id == UL_EP_INVALID || (uint32_t)id >= UL_CONFIG_MAX_ENDPOINTS)
		return NULL;
	if (!ep_pool[(uint32_t)id].active)
		return NULL;
	return &ep_pool[(uint32_t)id];
#else
	ul_endpoint_t *ep = (ul_endpoint_t *)(uintptr_t)id;

	if (!ep || !ep->active)
		return NULL;
	return ep;
#endif
}

#ifdef UL_UNIT_TEST
#include <ul/config.h>
ul_endpoint_t ep_pool[UL_CONFIG_MAX_ENDPOINTS];
#endif

void ul_ep_recv_queue_remove(ul_thread_t *th)
{
	ul_endpoint_t  *ep;
	ul_thread_t   **pp;

	if (th->blocked_ep == UL_EP_INVALID)
		return;

	ep = ul_ep_by_id(th->blocked_ep);
	if (!ep)
		return;

	pp = &ep->recv_queue;
	while (*pp && *pp != th)
		pp = &(*pp)->ipc_next;
	if (*pp) {
		*pp = th->ipc_next;
		th->ipc_next   = NULL;
		th->blocked_ep = UL_EP_INVALID;
	}
}

/* =========================================================================
 * Helpers
 * ========================================================================= */

static void ipc_enqueue_tail(ul_thread_t **head, ul_thread_t *th)
{
	ul_thread_t **pp = head;

	while (*pp)
		pp = &(*pp)->ipc_next;
	th->ipc_next = NULL;
	*pp = th;
}

static void deliver_to_server(ul_thread_t *server, ul_thread_t *caller,
			      const ul_msg_t *msg)
{
	server->ipc_msg    = *msg;
	server->ipc_sender = caller->tid;

	server->saved_prio = server->priority;
	if (caller->priority < server->priority)
		server->priority = caller->priority;

	server->state          = UL_THREAD_STATE_READY;
	server->blocked_reason = UL_BLOCKED_NONE;
	server->blocked_ep     = UL_EP_INVALID;

	if (server->blocked_notif != UL_NOTIF_INVALID) {
		ul_notif_obj_t *n = ul_notif_by_id(server->blocked_notif);

		if (n && n->waiter == server)
			n->waiter = NULL;
		server->blocked_notif = UL_NOTIF_INVALID;
	}

	ul_sched_enqueue(server);
}

/* =========================================================================
 * Core implementation (_impl functions — native pointer types)
 * ========================================================================= */

int ep_call_impl(ul_ep_t ep_id, ul_msg_t *msg)
{
	ul_endpoint_t *ep;
	ul_thread_t   *cur;

	if (!msg)
		return -UL_EINVAL;

	ep = ul_ep_by_id(ep_id);
	if (!ep)
		return -UL_EINVAL;

	cur = ul_sched_current();
	if (!cur)
		return -UL_EINVAL;

	cur->ipc_msg           = *msg;
	cur->ipc_sender        = UL_TID_INVALID;
	cur->blocked_reason    = UL_BLOCKED_IPC_CALL;
	cur->blocked_ep        = ep_id;
	cur->ipc_msg_outptr    = msg;

	if (ep->recv_queue) {
		ul_thread_t *srv = ep->recv_queue;

		ep->recv_queue = srv->ipc_next;
		deliver_to_server(srv, cur, &cur->ipc_msg);
	} else {
		ipc_enqueue_tail(&ep->send_queue, cur);
	}

	cur->state = UL_THREAD_STATE_BLOCKED;
	ul_sched_dequeue(cur);
	ul_sched_schedule();

	/* Re-fetch cur: local var may be stale after context switch. */
	cur = ul_sched_current();
	if (cur && cur->ipc_msg_outptr) {
		*cur->ipc_msg_outptr = cur->ipc_msg;
		cur->ipc_msg_outptr  = NULL;
	}
	return 0;
}

int ep_recv_impl(ul_ep_t ep_id, ul_msg_t *msg, ul_tid_t *sender)
{
	ul_endpoint_t *ep;
	ul_thread_t   *cur;

	if (!msg)
		return -UL_EINVAL;

	ep = ul_ep_by_id(ep_id);
	if (!ep)
		return -UL_EINVAL;

	cur = ul_sched_current();
	if (!cur)
		return -UL_EINVAL;

	if (ep->send_queue) {
		ul_thread_t *caller = ep->send_queue;

		ep->send_queue   = caller->ipc_next;
		caller->ipc_next = NULL;

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
	ul_sched_dequeue(cur);
	ul_sched_schedule();

	cur = ul_sched_current();
	if (!cur)
		return -UL_EINVAL;

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

int ep_reply_impl(ul_tid_t sender_tid, const ul_msg_t *reply)
{
	ul_thread_t *cur    = ul_sched_current();
	ul_thread_t *caller;

	if (!reply)
		return -UL_EINVAL;

	caller = ul_thread_by_tid(sender_tid);
	if (!caller || caller->blocked_reason != UL_BLOCKED_IPC_CALL)
		return -UL_EINVAL;

	if (cur)
		cur->priority = cur->saved_prio;

	caller->ipc_msg        = *reply;
	caller->state          = UL_THREAD_STATE_READY;
	caller->blocked_reason = UL_BLOCKED_NONE;
	caller->blocked_ep     = UL_EP_INVALID;
	ul_sched_enqueue(caller);

	return 0;
}

int ep_reply_recv_impl(ul_ep_t ep_id, ul_tid_t sender_tid,
		       const ul_msg_t *reply, ul_msg_t *next,
		       ul_tid_t *next_sender)
{
	if (!next)
		return -UL_EINVAL;

	if (sender_tid != UL_TID_INVALID)
		ep_reply_impl(sender_tid, reply);

	return ep_recv_impl(ep_id, next, next_sender);
}

int ep_grant_impl(ul_ep_t ep_id, ul_tid_t target_tid)
{
	if (!ul_ep_by_id(ep_id))
		return -UL_EINVAL;
	if (!ul_thread_by_tid(target_tid))
		return -UL_EINVAL;
	return 0;
}

int ep_recv_or_notif_impl(ul_ep_t ep_id, ul_notif_t notif_id,
			  uint32_t mask, ul_recv_or_notif_result_t *res)
{
	ul_endpoint_t  *ep;
	ul_notif_obj_t *n;
	ul_thread_t    *cur;
	uint32_t        matched;

	if (!res)
		return -UL_EINVAL;

	ep = ul_ep_by_id(ep_id);
	if (!ep)
		return -UL_EINVAL;

	n = ul_notif_by_id(notif_id);
	if (!n)
		return -UL_EINVAL;

	cur = ul_sched_current();
	if (!cur)
		return -UL_EINVAL;

	matched = n->bits & mask;
	if (matched) {
		n->bits         &= ~matched;
		res->is_notif    = 1;
		res->notif_bits  = matched;
		res->sender      = UL_TID_INVALID;
		return 1;
	}

	if (ep->send_queue) {
		ul_thread_t *caller = ep->send_queue;

		ep->send_queue   = caller->ipc_next;
		caller->ipc_next = NULL;

		cur->ipc_sender = caller->tid;
		cur->ipc_msg    = caller->ipc_msg;

		res->is_notif = 0;
		res->msg      = cur->ipc_msg;
		res->sender   = cur->ipc_sender;
		return 0;
	}

	cur->ipc_sender        = UL_TID_INVALID;
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
	ul_sched_dequeue(cur);
	ul_sched_schedule();

	/* Re-fetch cur: local var may be stale after context switch. */
	cur = ul_sched_current();
	if (!cur)
		return -UL_EINVAL;

	res = cur->rn_result_outptr;
	cur->rn_result_outptr = NULL;

	if (!res)
		return -UL_EINVAL;

	if (cur->ipc_sender != UL_TID_INVALID) {
		res->is_notif   = 0;
		res->msg        = cur->ipc_msg;
		res->sender     = cur->ipc_sender;
		res->notif_bits = 0;
		return 0;
	} else {
		res->is_notif   = 1;
		res->notif_bits = cur->notif_received;
		res->sender     = UL_TID_INVALID;
		return 1;
	}
}

/* =========================================================================
 * Syscall ABI wrappers — cast uint32_t args, delegate to _impl.
 * ========================================================================= */

uint32_t ul_kern_ep_create(void)
{
#ifdef UL_UNIT_TEST
	uint32_t i;

	for (i = 0u; i < UL_CONFIG_MAX_ENDPOINTS; i++) {
		if (!ep_pool[i].active) {
			ul_ep_init(&ep_pool[i], (ul_ep_t)i);
			return (uint32_t)i;
		}
	}
	return (uint32_t)(int32_t)(-UL_ENOSPC);
#else
	ul_endpoint_t *ep = (ul_endpoint_t *)ul_heap_alloc(sizeof(ul_endpoint_t));

	if (!ep)
		return (uint32_t)(int32_t)(-UL_ENOMEM);
	ul_ep_init(ep, (ul_ep_t)(uintptr_t)ep);
	return (uint32_t)(uintptr_t)ep;
#endif
}

uint32_t ul_kern_ep_call(uint32_t ep_id, uint32_t msg_ptr)
{
	return (uint32_t)(int32_t)ep_call_impl(
		(ul_ep_t)ep_id,
		(ul_msg_t *)(uintptr_t)msg_ptr);
}

uint32_t ul_kern_ep_recv(uint32_t ep_id, uint32_t msg_ptr,
			 uint32_t sender_ptr)
{
	return (uint32_t)(int32_t)ep_recv_impl(
		(ul_ep_t)ep_id,
		(ul_msg_t  *)(uintptr_t)msg_ptr,
		(ul_tid_t  *)(uintptr_t)sender_ptr);
}

uint32_t ul_kern_ep_reply(uint32_t sender_tid_u32, uint32_t reply_ptr)
{
	return (uint32_t)(int32_t)ep_reply_impl(
		(ul_tid_t)sender_tid_u32,
		(const ul_msg_t *)(uintptr_t)reply_ptr);
}

uint32_t ul_kern_ep_reply_recv(uint32_t ep_id, uint32_t sender_tid_u32,
			       uint32_t args_ptr)
{
	const ul_reply_recv_args_t *args =
		(const ul_reply_recv_args_t *)(uintptr_t)args_ptr;

	if (!args)
		return (uint32_t)(int32_t)(-UL_EINVAL);

	return (uint32_t)(int32_t)ep_reply_recv_impl(
		(ul_ep_t)ep_id,
		(ul_tid_t)sender_tid_u32,
		args->reply, args->next, args->next_sender);
}

uint32_t ul_kern_ep_grant(uint32_t ep_id, uint32_t target_tid)
{
	return (uint32_t)(int32_t)ep_grant_impl(
		(ul_ep_t)ep_id, (ul_tid_t)target_tid);
}

uint32_t ul_kern_ep_recv_or_notif(uint32_t ep_id, uint32_t notif_id,
				  uint32_t mask, uint32_t result_ptr)
{
	return (uint32_t)(int32_t)ep_recv_or_notif_impl(
		(ul_ep_t)ep_id,
		(ul_notif_t)notif_id,
		mask,
		(ul_recv_or_notif_result_t *)(uintptr_t)result_ptr);
}

uint32_t ul_kern_ep_destroy(uint32_t ep_id)
{
#ifdef UL_UNIT_TEST
	uint32_t i = (uint32_t)ep_id;

	if (i >= UL_CONFIG_MAX_ENDPOINTS || !ep_pool[i].active)
		return (uint32_t)(int32_t)(-UL_EINVAL);
	ep_pool[i].active = false;
	return 0u;
#else
	ul_endpoint_t *ep = (ul_endpoint_t *)(uintptr_t)ep_id;

	if (!ep || !ep->active)
		return (uint32_t)(int32_t)(-UL_EINVAL);
	ep->active = false;
	ul_heap_free(ep);
	return 0u;
#endif
}
