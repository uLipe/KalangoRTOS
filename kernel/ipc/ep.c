/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * IPC endpoint handlers — kernel/ipc/ep.c
 * Implements: syscall_router.h ul_kern_ep_* prototypes
 * Reference: docs/api_spec.md §7, docs/microkernel_book_tricore.md §9
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <ul/syscall_abi.h>
#include <ul/config.h>
#include <kernel/include/ul_ep_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

uint32_t ul_kern_ep_create(void)
{
	/* TODO: allocate endpoint from static pool */
	return (uint32_t)(int32_t)UL_EP_INVALID;
}

uint32_t ul_kern_ep_call(uint32_t ep, uint32_t msg_ptr)
{
	(void)ep;
	(void)msg_ptr;
	/*
	 * TODO:
	 *   1. Find endpoint by id
	 *   2. Copy *msg_ptr into kernel buffer
	 *   3. If receiver is waiting: wake it up (transfer message, switch)
	 *   4. Else: block caller (insert into endpoint's pending queue)
	 */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_ep_recv(uint32_t ep, uint32_t msg_ptr, uint32_t sender_ptr)
{
	(void)ep;
	(void)msg_ptr;
	(void)sender_ptr;
	/*
	 * TODO:
	 *   1. Find endpoint by id
	 *   2. If caller waiting: copy message to *msg_ptr, write tid to *sender_ptr
	 *   3. Else: block receiver
	 */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_ep_reply(uint32_t sender_tid, uint32_t reply_ptr)
{
	(void)sender_tid;
	(void)reply_ptr;
	/* TODO: copy *reply_ptr to blocked caller, wake it up */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_ep_reply_recv(uint32_t ep, uint32_t sender_tid,
			       uint32_t args_ptr)
{
	const ul_reply_recv_args_t *args =
		(const ul_reply_recv_args_t *)(uintptr_t)args_ptr;
	(void)ep;
	(void)sender_tid;
	(void)args;
	/* TODO: atomic reply-then-wait (fast path in seL4 model) */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_ep_grant(uint32_t ep, uint32_t target_tid)
{
	(void)ep;
	(void)target_tid;
	/* TODO: add ep to target thread's capability set */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_ep_recv_or_notif(uint32_t ep, uint32_t notif,
				  uint32_t mask, uint32_t result_ptr)
{
	ul_recv_or_notif_result_t *res =
		(ul_recv_or_notif_result_t *)(uintptr_t)result_ptr;
	(void)ep;
	(void)notif;
	(void)mask;
	(void)res;
	/*
	 * TODO:
	 *   If notification bits already set: fill res->notif_bits, return 1
	 *   Else if IPC available: fill res->msg and res->sender, return 0
	 *   Else: block until either fires
	 */
	return (uint32_t)(int32_t)UL_EINVAL;
}
