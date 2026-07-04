/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Bitmap priority scheduler — kernel/sched/bitmap_rt.c
 *
 * Policy: strict priority (lower value = higher priority, 0 = most urgent).
 * Within the same priority level, threads are served FIFO.
 *
 * Data structure: 256-slot bitmap (8 × uint32_t) + sys_dlist_t per level.
 * enqueue/dequeue/remove are O(1); pick_next/peek_next use bitmap_first_set.
 */

#include <stddef.h>
#include <kernel/include/ulmk_thread_internal.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/include/list.h>
#include <ulmk_arch.h>

#define BITMAP_WORDS	8u		/* 8 × 32 = 256 priority levels */

static uint32_t  bitmap[BITMAP_WORDS];
static sys_dlist_t level[256];

static uint8_t bitmap_first_set(void)
{
	uint32_t i;
	uint32_t w;

	for (i = 0u; i < BITMAP_WORDS; i++) {
		w = bitmap[i];
		if (w) {
			return (uint8_t)(i * 32u +
					 31u - ulmk_arch_cpu_clz(w & (uint32_t)(-(int32_t)w)));
		}
	}
	return 255u;
}

static void bitmap_set(uint8_t prio)
{
	bitmap[prio >> 5u] |= (1u << (prio & 31u));
}

static void bitmap_clear(uint8_t prio)
{
	bitmap[prio >> 5u] &= ~(1u << (prio & 31u));
}

static void bitmap_rt_init(void)
{
	uint32_t i;

	for (i = 0u; i < BITMAP_WORDS; i++)
		bitmap[i] = 0u;
	for (i = 0u; i < 256u; i++)
		sys_dlist_init(&level[i]);
}

static void bitmap_rt_enqueue(ulmk_thread_t *t)
{
	uint8_t p = t->priority;

	sys_dlist_append(&level[p], &t->sched_node);
	bitmap_set(p);
	t->state = UL_THREAD_STATE_READY;
}

static void bitmap_rt_dequeue(ulmk_thread_t *t)
{
	uint8_t p = t->priority;

	if (!sys_dnode_is_linked(&t->sched_node))
		return;

	sys_dlist_remove(&t->sched_node);
	sys_dnode_init(&t->sched_node);

	if (sys_dlist_is_empty(&level[p]))
		bitmap_clear(p);
}

static ulmk_thread_t *bitmap_rt_pick_next(void)
{
	uint8_t        p;
	sys_dnode_t   *dn;
	ulmk_thread_t *t;

	p = bitmap_first_set();
	dn = sys_dlist_get(&level[p]);
	if (!dn)
		return NULL;

	if (sys_dlist_is_empty(&level[p]))
		bitmap_clear(p);

	t = SYS_DLIST_CONTAINER_OF(dn, ulmk_thread_t, sched_node);
	sys_dnode_init(&t->sched_node);
	return t;
}

static ulmk_thread_t *bitmap_rt_peek_next(void)
{
	uint8_t p = bitmap_first_set();

	return SYS_DLIST_PEEK_HEAD_CONTAINER_OF(&level[p], ulmk_thread_t, sched_node);
}

const ulmk_sched_class_t ulmk_bitmap_rt_class = {
	.name      = "bitmap-rt",
	.init      = bitmap_rt_init,
	.enqueue   = bitmap_rt_enqueue,
	.dequeue   = bitmap_rt_dequeue,
	.pick_next = bitmap_rt_pick_next,
	.peek_next = bitmap_rt_peek_next,
};
