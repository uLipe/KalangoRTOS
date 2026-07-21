/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal IPC endpoint management.
 */

#ifndef UL_EP_INTERNAL_H
#define UL_EP_INTERNAL_H

#include <stdbool.h>
#include <ulmk/microkernel.h>
#include <kernel/include/list.h>
#include <kernel/include/ulmk_thread_internal.h>

typedef struct ulmk_endpoint {
	ulmk_ep_t      id;
	bool         active;
	sys_dlist_t  send_queue;	/* callers blocked (BLOCKED_IPC_CALL) */
	sys_dlist_t  recv_queue;	/* servers blocked (BLOCKED_IPC_RECV) */
} ulmk_endpoint_t;

int           ulmk_ep_init(ulmk_endpoint_t *ep, ulmk_ep_t id);
ulmk_endpoint_t *ulmk_ep_by_id(ulmk_ep_t id);

/*
 * Remove @th from the recv_queue of the endpoint it is blocking on.
 * Used by ulmk_kern_thread_kill to avoid dangling queue entries.
 */
void          ulmk_ep_recv_queue_remove(ulmk_thread_t *th);

/*
 * Core IPC logic with native pointer types.
 * Used directly by unit tests to avoid 32-bit ABI truncation on 64-bit hosts.
 * The ulmk_kern_ep_* syscall handlers are thin wrappers around these.
 */
int  ep_call_impl(ulmk_ep_t ep_id, ulmk_msg_t *msg);
int  ep_call_timeout_impl(ulmk_ep_t ep_id, ulmk_msg_t *msg, uint32_t timeout_ms);
int  ep_recv_impl(ulmk_ep_t ep_id, ulmk_msg_t *msg, ulmk_tid_t *sender);
int  ep_reply_impl(ulmk_tid_t sender_tid, const ulmk_msg_t *reply);
int  ep_reply_recv_impl(ulmk_ep_t ep_id, ulmk_tid_t sender_tid,
			const ulmk_msg_t *reply, ulmk_msg_t *next,
			ulmk_tid_t *next_sender);
int  ep_grant_impl(ulmk_ep_t ep_id, ulmk_tid_t target_tid);
int  ep_recv_or_notif_impl(ulmk_ep_t ep_id, ulmk_notif_t notif_id,
			   uint32_t mask, ulmk_recv_or_notif_result_t *res);
int  ep_destroy_impl(ulmk_ep_t ep_id);

#endif /* UL_EP_INTERNAL_H */
