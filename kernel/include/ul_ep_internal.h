/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal IPC endpoint management.
 */

#ifndef UL_EP_INTERNAL_H
#define UL_EP_INTERNAL_H

#include <stdbool.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_thread_internal.h>

typedef struct ul_endpoint {
	ul_ep_t      id;
	bool         active;
	ul_thread_t *send_queue;	/* callers blocked (BLOCKED_IPC_CALL) */
	ul_thread_t *recv_queue;	/* servers blocked (BLOCKED_IPC_RECV) */
} ul_endpoint_t;

int           ul_ep_init(ul_endpoint_t *ep, ul_ep_t id);
ul_endpoint_t *ul_ep_by_id(ul_ep_t id);

/*
 * Remove @th from the recv_queue of the endpoint it is blocking on.
 * Used by ul_kern_thread_kill to avoid dangling queue entries.
 */
void          ul_ep_recv_queue_remove(ul_thread_t *th);

/*
 * Core IPC logic with native pointer types.
 * Used directly by unit tests to avoid 32-bit ABI truncation on 64-bit hosts.
 * The ul_kern_ep_* syscall handlers are thin wrappers around these.
 */
int  ep_call_impl(ul_ep_t ep_id, ul_msg_t *msg);
int  ep_recv_impl(ul_ep_t ep_id, ul_msg_t *msg, ul_tid_t *sender);
int  ep_reply_impl(ul_tid_t sender_tid, const ul_msg_t *reply);
int  ep_reply_recv_impl(ul_ep_t ep_id, ul_tid_t sender_tid,
			const ul_msg_t *reply, ul_msg_t *next,
			ul_tid_t *next_sender);
int  ep_grant_impl(ul_ep_t ep_id, ul_tid_t target_tid);
int  ep_recv_or_notif_impl(ul_ep_t ep_id, ul_notif_t notif_id,
			   uint32_t mask, ul_recv_or_notif_result_t *res);

#endif /* UL_EP_INTERNAL_H */
