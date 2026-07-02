/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Bitmap priority scheduler — kernel/sched/bitmap_rt.c
 *
 * Policy: strict priority (lower value = higher priority, 0 = most urgent).
 * Within the same priority level, threads are served FIFO.
 *
 * Data structure: 256-slot bitmap (8 × uint32_t) + doubly-linked list per
 * priority level.  All operations are O(1):
 *
 *   enqueue()  — set bitmap bit, append to tail of level list
 *   dequeue()  — remove via prev/next pointers, clear bitmap bit if level empty
 *   pick_next()— find lowest set bit (highest priority), pop from head
 *   peek_next()— find lowest set bit, return head without removing
 *
 * The TriCore CLZ instruction makes "find first set" O(1) with a constant
 * 8-word scan of the bitmap.
 */

#include <stddef.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>
#include <ul_arch.h>

#define BITMAP_WORDS	8u		/* 8 × 32 = 256 priority levels */

static uint32_t     bitmap[BITMAP_WORDS];
static ul_thread_t *level_head[256];
static ul_thread_t *level_tail[256];

/*
 * Return the priority of the highest-priority ready thread (lowest bit set
 * in the bitmap), or 255 if the bitmap is empty.
 *
 * Scans 8 words; the first non-zero word is checked with CLZ to find the
 * least-significant set bit (lowest priority number = most urgent).
 * w & (-w) isolates that bit; CLZ(w & -w) = 31 - bit_position for uint32_t.
 */
static uint8_t bitmap_first_set(void)
{
	uint32_t i;
	uint32_t w;

	for (i = 0u; i < BITMAP_WORDS; i++) {
		w = bitmap[i];
		if (w) {
			/*
			 * w & (-w) isolates the lowest set bit.
			 * CLZ on that value gives (31 - bit_index).
			 * bit_index = 31 - CLZ(w & -w).
			 */
			return (uint8_t)(i * 32u +
					 31u - ul_arch_cpu_clz(w & (uint32_t)(-(int32_t)w)));
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
	for (i = 0u; i < 256u; i++) {
		level_head[i] = NULL;
		level_tail[i] = NULL;
	}
}

static void bitmap_rt_enqueue(ul_thread_t *t)
{
	uint8_t p = t->priority;

	t->next       = NULL;
	t->sched_prev = level_tail[p];

	if (level_tail[p]) {
		level_tail[p]->next = t;
	} else {
		level_head[p] = t;
	}
	level_tail[p] = t;
	bitmap_set(p);
	t->state = UL_THREAD_STATE_READY;
}

static void bitmap_rt_dequeue(ul_thread_t *t)
{
	uint8_t p = t->priority;

	/*
	 * Guard: if the thread has no predecessor and is not the level head,
	 * it is not in the queue (e.g. it was already removed by pick_next).
	 * This matches fifo_rt's scan-to-find semantics and prevents
	 * accidentally corrupting level_head when dequeue is called on the
	 * currently-running thread.
	 */
	if (!t->sched_prev && level_head[p] != t)
		return;

	if (t->sched_prev)
		t->sched_prev->next = t->next;
	else
		level_head[p] = t->next;

	if (t->next)
		t->next->sched_prev = t->sched_prev;
	else
		level_tail[p] = t->sched_prev;

	t->next       = NULL;
	t->sched_prev = NULL;

	if (!level_head[p])
		bitmap_clear(p);
}

static ul_thread_t *bitmap_rt_pick_next(void)
{
	uint8_t      p;
	ul_thread_t *t;

	p = bitmap_first_set();
	t = level_head[p];
	if (!t)
		return NULL;

	level_head[p] = t->next;
	if (level_head[p])
		level_head[p]->sched_prev = NULL;
	else
		level_tail[p] = NULL;

	if (!level_head[p])
		bitmap_clear(p);

	t->next       = NULL;
	t->sched_prev = NULL;
	return t;
}

static ul_thread_t *bitmap_rt_peek_next(void)
{
	return level_head[bitmap_first_set()];
}

const ul_sched_class_t ul_bitmap_rt_class = {
	.name      = "bitmap-rt",
	.init      = bitmap_rt_init,
	.enqueue   = bitmap_rt_enqueue,
	.dequeue   = bitmap_rt_dequeue,
	.pick_next = bitmap_rt_pick_next,
	.peek_next = bitmap_rt_peek_next,
};
