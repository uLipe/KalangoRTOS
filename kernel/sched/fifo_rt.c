/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * RT-FIFO scheduling policy — kernel/sched/fifo_rt.c
 *
 * Policy: strict priority (lower value = higher priority).
 * Within the same priority level, threads are served FIFO.
 *
 * Data structure: single singly-linked list sorted by ascending priority.
 * Head = highest-priority ready thread → pick_next() is O(1).
 * enqueue() inserts at the tail of the same-priority group → O(n).
 * dequeue() removes by pointer scan → O(n).
 * Both are acceptable for UL_CONFIG_MAX_THREADS ≤ 32.
 */

#include <stddef.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>

static ul_thread_t *rq_head;

static void fifo_rt_init(void)
{
	rq_head = NULL;
}

/*
 * Insert t at the tail of its priority group.
 * Threads with strictly lower priority number (higher urgency) stay ahead.
 * Threads with equal or higher priority number (same or lower urgency)
 * stay behind.
 */
static void fifo_rt_enqueue(ul_thread_t *t)
{
	ul_thread_t **pp = &rq_head;

	while (*pp && (*pp)->priority <= t->priority)
		pp = &(*pp)->next;

	t->next = *pp;
	*pp = t;
	t->state = UL_THREAD_STATE_READY;
}

static void fifo_rt_dequeue(ul_thread_t *t)
{
	ul_thread_t **pp = &rq_head;

	while (*pp && *pp != t)
		pp = &(*pp)->next;

	if (*pp == t)
		*pp = t->next;

	t->next = NULL;
}

static ul_thread_t *fifo_rt_pick_next(void)
{
	return rq_head;
}

const ul_sched_class_t ul_fifo_rt_class = {
	.name      = "fifo-rt",
	.init      = fifo_rt_init,
	.enqueue   = fifo_rt_enqueue,
	.dequeue   = fifo_rt_dequeue,
	.pick_next = fifo_rt_pick_next,
};
