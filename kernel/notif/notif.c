/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Notification handlers — kernel/notif/notif.c
 * Reference: docs/api_spec.md §8
 *
 * Two-layer design:
 *   notif_*_impl() — core logic with native C pointer types; testable on host.
 *   ulmk_kern_notif_*() — syscall ABI wrappers.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_notif_internal.h>
#include <kernel/include/ulmk_ep_internal.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/syscall/syscall_router.h>
#include <ulmk_arch.h>

#ifndef UL_UNIT_TEST
#include <kernel/include/ulmk_mem_internal.h>
#else
#include <ulmk/config.h>
#endif

#ifdef UL_UNIT_TEST
ulmk_notif_obj_t notif_pool[ULMK_CONFIG_MAX_NOTIFS];
#endif

int ulmk_notif_obj_init(ulmk_notif_obj_t *n, ulmk_notif_t id)
{
	n->id        = id;
	n->active    = true;
	n->bits      = 0;
	n->waiter    = NULL;
	n->wait_mask = 0;
	return 0;
}

ulmk_notif_obj_t *ulmk_notif_by_id(ulmk_notif_t id)
{
#ifdef UL_UNIT_TEST
	if (id == ULMK_NOTIF_INVALID || (uint32_t)id >= ULMK_CONFIG_MAX_NOTIFS)
		return NULL;
	if (!notif_pool[(uint32_t)id].active)
		return NULL;
	return &notif_pool[(uint32_t)id];
#else
	ulmk_notif_obj_t *n = (ulmk_notif_obj_t *)(uintptr_t)id;

	if (!n || !n->active)
		return NULL;
	return n;
#endif
}

/* =========================================================================
 * Core implementation (_impl — native pointer types)
 * ========================================================================= */

int notif_signal_impl(ulmk_notif_t notif_id, uint32_t bits)
{
	ulmk_arch_irq_key_t  key;
	ulmk_notif_obj_t    *n = ulmk_notif_by_id(notif_id);
	ulmk_thread_t       *w;
	uint32_t           delivered;

	if (!n)
		return -ULMK_EINVAL;

	key = ulmk_arch_cpu_irq_save();

	n->bits |= bits;

	w = n->waiter;
	if (!w || w->state != UL_THREAD_STATE_BLOCKED) {
		ulmk_arch_cpu_irq_restore(key);
		return 0;
	}

	delivered = n->bits & w->notif_wait_mask;
	if (!delivered) {
		ulmk_arch_cpu_irq_restore(key);
		return 0;
	}

	n->bits          &= ~delivered;
	n->waiter         = NULL;
	w->notif_received = delivered;

	if (w->blocked_reason == UL_BLOCKED_IPC_OR_NOTIF) {
		ulmk_ep_recv_queue_remove(w);
		w->blocked_notif = ULMK_NOTIF_INVALID;
	}

	w->blocked_reason = UL_BLOCKED_NONE;
	w->state          = UL_THREAD_STATE_READY;
	ulmk_sched_enqueue(w);

	ulmk_arch_cpu_irq_restore(key);

	return 0;
}

int notif_wait_impl(ulmk_notif_t notif_id, uint32_t mask, uint32_t *out)
{
	ulmk_arch_irq_key_t  key;
	ulmk_notif_obj_t    *n = ulmk_notif_by_id(notif_id);
	ulmk_thread_t       *cur;
	uint32_t           matched;

	if (!n || !out)
		return -ULMK_EINVAL;

	key = ulmk_arch_cpu_irq_save();

	matched = n->bits & mask;
	if (matched) {
		n->bits &= ~matched;
		ulmk_arch_cpu_irq_restore(key);
		*out = matched;
		return 0;
	}

	cur = ulmk_sched_current();
	if (!cur) {
		ulmk_arch_cpu_irq_restore(key);
		return -ULMK_EINVAL;
	}

	cur->blocked_reason    = UL_BLOCKED_NOTIF;
	cur->notif_wait_mask   = mask;
	cur->blocked_notif     = notif_id;
	cur->notif_bits_outptr = out;

	n->waiter    = cur;
	n->wait_mask = mask;

	cur->state = UL_THREAD_STATE_BLOCKED;
	ulmk_sched_dequeue(cur);
	ulmk_arch_cpu_irq_restore(key);

	ulmk_sched_schedule();

	/* Re-fetch cur: local var may be stale after context switch. */
	cur = ulmk_sched_current();
	if (!cur)
		return -ULMK_EINVAL;

	if (cur->notif_bits_outptr) {
		*cur->notif_bits_outptr = cur->notif_received;
		cur->notif_bits_outptr  = NULL;
	}
	return 0;
}

uint32_t notif_poll_impl(ulmk_notif_t notif_id, uint32_t mask)
{
	ulmk_arch_irq_key_t  key;
	ulmk_notif_obj_t    *n = ulmk_notif_by_id(notif_id);
	uint32_t           matched;

	if (!n)
		return 0;

	key = ulmk_arch_cpu_irq_save();
	matched  = n->bits & mask;
	n->bits &= ~matched;
	ulmk_arch_cpu_irq_restore(key);

	return matched;
}

/* =========================================================================
 * Syscall ABI wrappers
 * ========================================================================= */

uint32_t ulmk_kern_notif_create(void)
{
#ifdef UL_UNIT_TEST
	uint32_t i;

	for (i = 0u; i < ULMK_CONFIG_MAX_NOTIFS; i++) {
		if (!notif_pool[i].active) {
			ulmk_notif_obj_init(&notif_pool[i], (ulmk_notif_t)i);
			return (uint32_t)i;
		}
	}
	return (uint32_t)(int32_t)(-ULMK_ENOSPC);
#else
	ulmk_notif_obj_t *n = (ulmk_notif_obj_t *)ulmk_heap_alloc(sizeof(ulmk_notif_obj_t));

	if (!n)
		return (uint32_t)(int32_t)(-ULMK_ENOMEM);
	ulmk_notif_obj_init(n, (ulmk_notif_t)(uintptr_t)n);
	return (uint32_t)(uintptr_t)n;
#endif
}

uint32_t ulmk_kern_notif_destroy(uint32_t notif_id)
{
#ifdef UL_UNIT_TEST
	uint32_t i = notif_id;

	if (i >= ULMK_CONFIG_MAX_NOTIFS || !notif_pool[i].active)
		return (uint32_t)(int32_t)(-ULMK_EINVAL);
	notif_pool[i].active = false;
	return 0u;
#else
	ulmk_notif_obj_t *n = (ulmk_notif_obj_t *)(uintptr_t)notif_id;

	if (!n || !n->active)
		return (uint32_t)(int32_t)(-ULMK_EINVAL);
	n->active = false;
	ulmk_heap_free(n);
	return 0u;
#endif
}

uint32_t ulmk_kern_notif_signal(uint32_t notif_id, uint32_t bits)
{
	return (uint32_t)(int32_t)notif_signal_impl((ulmk_notif_t)notif_id, bits);
}

uint32_t ulmk_kern_notif_wait(uint32_t notif_id, uint32_t mask,
			    uint32_t bits_ptr)
{
	return (uint32_t)(int32_t)notif_wait_impl(
		(ulmk_notif_t)notif_id, mask,
		(uint32_t *)(uintptr_t)bits_ptr);
}

uint32_t ulmk_kern_notif_poll(uint32_t notif_id, uint32_t mask)
{
	return notif_poll_impl((ulmk_notif_t)notif_id, mask);
}
