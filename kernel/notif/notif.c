/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Notification handlers — kernel/notif/notif.c
 * Reference: docs/api_spec.md §8, docs/microkernel_book_tricore.md §9
 *
 * Two-layer design:
 *   notif_*_impl() — core logic with native C pointer types; testable on host.
 *   ul_kern_notif_*() — syscall ABI wrappers.
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/include/ul_notif_internal.h>
#include <kernel/include/ul_ep_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

ul_notif_obj_t notif_pool[UL_CONFIG_MAX_NOTIFS];

int ul_notif_obj_init(ul_notif_obj_t *n, ul_notif_t id)
{
	n->id        = id;
	n->active    = true;
	n->bits      = 0;
	n->waiter    = NULL;
	n->wait_mask = 0;
	return 0;
}

ul_notif_obj_t *ul_notif_by_id(ul_notif_t id)
{
	if (id < 0 || id >= (ul_notif_t)UL_CONFIG_MAX_NOTIFS)
		return NULL;
	if (!notif_pool[id].active)
		return NULL;
	return &notif_pool[id];
}

/* =========================================================================
 * Core implementation (_impl — native pointer types)
 * ========================================================================= */

int notif_signal_impl(ul_notif_t notif_id, uint32_t bits)
{
	ul_arch_irq_key_t  key;
	ul_notif_obj_t    *n = ul_notif_by_id(notif_id);
	ul_thread_t       *w;
	uint32_t           delivered;

	if (!n)
		return -UL_EINVAL;

	key = ul_arch_cpu_irq_save();

	n->bits |= bits;

	w = n->waiter;
	if (!w || w->state != UL_THREAD_STATE_BLOCKED) {
		ul_arch_cpu_irq_restore(key);
		return 0;
	}

	delivered = n->bits & w->notif_wait_mask;
	if (!delivered) {
		ul_arch_cpu_irq_restore(key);
		return 0;
	}

	n->bits          &= ~delivered;
	n->waiter         = NULL;
	w->notif_received = delivered;

	if (w->blocked_reason == UL_BLOCKED_IPC_OR_NOTIF) {
		ul_ep_recv_queue_remove(w);
		w->blocked_notif = UL_NOTIF_INVALID;
	}

	w->blocked_reason = UL_BLOCKED_NONE;
	w->state          = UL_THREAD_STATE_READY;
	ul_sched_enqueue(w);

	ul_arch_cpu_irq_restore(key);

	return 0;
}

int notif_wait_impl(ul_notif_t notif_id, uint32_t mask, uint32_t *out)
{
	ul_arch_irq_key_t  key;
	ul_notif_obj_t    *n = ul_notif_by_id(notif_id);
	ul_thread_t       *cur;
	uint32_t           matched;

	if (!n || !out)
		return -UL_EINVAL;

	key = ul_arch_cpu_irq_save();

	matched = n->bits & mask;
	if (matched) {
		n->bits &= ~matched;
		ul_arch_cpu_irq_restore(key);
		*out = matched;
		return 0;
	}

	cur = ul_sched_current();
	if (!cur) {
		ul_arch_cpu_irq_restore(key);
		return -UL_EINVAL;
	}

	cur->blocked_reason    = UL_BLOCKED_NOTIF;
	cur->notif_wait_mask   = mask;
	cur->blocked_notif     = notif_id;
	cur->notif_bits_outptr = out;

	n->waiter    = cur;
	n->wait_mask = mask;

	cur->state = UL_THREAD_STATE_BLOCKED;
	ul_sched_dequeue(cur);
	ul_arch_cpu_irq_restore(key);

	ul_sched_schedule();

	/* Re-fetch cur: local var may be stale after context switch. */
	cur = ul_sched_current();
	if (!cur)
		return -UL_EINVAL;

	if (cur->notif_bits_outptr) {
		*cur->notif_bits_outptr = cur->notif_received;
		cur->notif_bits_outptr  = NULL;
	}
	return 0;
}

uint32_t notif_poll_impl(ul_notif_t notif_id, uint32_t mask)
{
	ul_arch_irq_key_t  key;
	ul_notif_obj_t    *n = ul_notif_by_id(notif_id);
	uint32_t           matched;

	if (!n)
		return 0;

	key = ul_arch_cpu_irq_save();
	matched  = n->bits & mask;
	n->bits &= ~matched;
	ul_arch_cpu_irq_restore(key);

	return matched;
}

/* =========================================================================
 * Syscall ABI wrappers
 * ========================================================================= */

uint32_t ul_kern_notif_create(void)
{
	ul_notif_t i;

	for (i = 0; i < (ul_notif_t)UL_CONFIG_MAX_NOTIFS; i++) {
		if (!notif_pool[i].active) {
			ul_notif_obj_init(&notif_pool[i], i);
			return (uint32_t)i;
		}
	}
	return (uint32_t)(int32_t)(-UL_ENOSPC);
}

uint32_t ul_kern_notif_signal(uint32_t notif_id, uint32_t bits)
{
	return (uint32_t)(int32_t)notif_signal_impl((ul_notif_t)notif_id, bits);
}

uint32_t ul_kern_notif_wait(uint32_t notif_id, uint32_t mask,
			    uint32_t bits_ptr)
{
	return (uint32_t)(int32_t)notif_wait_impl(
		(ul_notif_t)notif_id, mask,
		(uint32_t *)(uintptr_t)bits_ptr);
}

uint32_t ul_kern_notif_poll(uint32_t notif_id, uint32_t mask)
{
	return notif_poll_impl((ul_notif_t)notif_id, mask);
}
