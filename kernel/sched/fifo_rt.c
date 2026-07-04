/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * RT-FIFO scheduling policy — kernel/sched/fifo_rt.c
 *
 * Policy: strict priority (lower value = higher priority).
 * Within the same priority level, threads are served FIFO.
 *
 * Not used in production builds (bitmap_rt is the default).  Kept for
 * sched_unit tests and as a reference policy implementation.
 */

#include <stddef.h>
#include <kernel/include/ulmk_thread_internal.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/include/list.h>

static sys_dlist_t rq;

static int fifo_prio_cond(sys_dnode_t *node, void *data)
{
	ulmk_thread_t *t = (ulmk_thread_t *)data;
	ulmk_thread_t *n = SYS_DLIST_CONTAINER_OF(node, ulmk_thread_t, sched_node);

	return n->priority > t->priority;
}

static void fifo_rt_init(void)
{
	sys_dlist_init(&rq);
}

static void fifo_rt_enqueue(ulmk_thread_t *t)
{
	sys_dlist_insert_at(&rq, &t->sched_node, fifo_prio_cond, t);
	t->state = UL_THREAD_STATE_READY;
}

static void fifo_rt_dequeue(ulmk_thread_t *t)
{
	if (!sys_dnode_is_linked(&t->sched_node))
		return;

	sys_dlist_remove(&t->sched_node);
	sys_dnode_init(&t->sched_node);
}

static ulmk_thread_t *fifo_rt_pick_next(void)
{
	return SYS_DLIST_PEEK_HEAD_CONTAINER_OF(&rq, ulmk_thread_t, sched_node);
}

static ulmk_thread_t *fifo_rt_peek_next(void)
{
	return SYS_DLIST_PEEK_HEAD_CONTAINER_OF(&rq, ulmk_thread_t, sched_node);
}

const ulmk_sched_class_t ulmk_fifo_rt_class = {
	.name      = "fifo-rt",
	.init      = fifo_rt_init,
	.enqueue   = fifo_rt_enqueue,
	.dequeue   = fifo_rt_dequeue,
	.pick_next = fifo_rt_pick_next,
	.peek_next = fifo_rt_peek_next,
};
