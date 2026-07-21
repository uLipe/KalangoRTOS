/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Bitmap priority scheduler — kernel/sched/bitmap_rt.c
 *
 * One O(1) runqueue per CPU.  Affinity is permanent (t->cpu); migrate is out
 * of scope.  pick_next/peek_next always examine the local CPU's queue.
 */

#include <stddef.h>
#include <ulmk/config.h>
#include <kernel/include/ulmk_thread_internal.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/include/ulmk_percpu.h>
#include <kernel/include/list.h>
#include <ulmk_arch.h>

#define BITMAP_WORDS	8u		/* 8 × 32 = 256 priority levels */

struct cpu_rq {
	uint32_t   bitmap[BITMAP_WORDS];
	sys_dlist_t level[256];
};

static struct cpu_rq rq[ULMK_NR_CPUS];
/*
 * Protects all RQ mutation.  Remote CPUs enqueue under this lock; the owner
 * CPU pick_next/dequeue also take it — required for SMP without migration.
 */
static ulmk_spinlock_t rq_lock = ULMK_SPINLOCK_INIT;

static uint8_t bitmap_first_set(const struct cpu_rq *q)
{
	uint32_t i;
	uint32_t w;

	for (i = 0u; i < BITMAP_WORDS; i++) {
		w = q->bitmap[i];
		if (w) {
			return (uint8_t)(i * 32u +
					 31u - ulmk_arch_cpu_clz(w & (uint32_t)(-(int32_t)w)));
		}
	}
	return 255u;
}

static void bitmap_set(struct cpu_rq *q, uint8_t prio)
{
	q->bitmap[prio >> 5u] |= (1u << (prio & 31u));
}

static void bitmap_clear(struct cpu_rq *q, uint8_t prio)
{
	q->bitmap[prio >> 5u] &= ~(1u << (prio & 31u));
}

static struct cpu_rq *rq_of_cpu(uint8_t cpu)
{
	if (cpu >= (uint8_t)ULMK_NR_CPUS)
		cpu = 0u;
	return &rq[cpu];
}

static void bitmap_rt_init(void)
{
	uint32_t c;
	uint32_t i;

	for (c = 0u; c < (uint32_t)ULMK_NR_CPUS; c++) {
		for (i = 0u; i < BITMAP_WORDS; i++)
			rq[c].bitmap[i] = 0u;
		for (i = 0u; i < 256u; i++)
			sys_dlist_init(&rq[c].level[i]);
	}
}

static void bitmap_rt_enqueue(ulmk_thread_t *t)
{
	ulmk_arch_irq_key_t key = ulmk_arch_spin_lock_irqsave(&rq_lock);
	struct cpu_rq *q = rq_of_cpu(t->cpu);
	uint8_t        p = t->priority;

	if (!sys_dnode_is_linked(&t->sched_node)) {
		sys_dlist_append(&q->level[p], &t->sched_node);
		bitmap_set(q, p);
	}
	t->state = UL_THREAD_STATE_READY;
	ulmk_arch_spin_unlock_irqrestore(&rq_lock, key);
}

static void bitmap_rt_dequeue(ulmk_thread_t *t)
{
	ulmk_arch_irq_key_t key = ulmk_arch_spin_lock_irqsave(&rq_lock);
	struct cpu_rq *q = rq_of_cpu(t->cpu);
	uint8_t        p = t->priority;

	if (sys_dnode_is_linked(&t->sched_node)) {
		sys_dlist_remove(&t->sched_node);
		sys_dnode_init(&t->sched_node);

		if (sys_dlist_is_empty(&q->level[p]))
			bitmap_clear(q, p);
	}
	ulmk_arch_spin_unlock_irqrestore(&rq_lock, key);
}

static ulmk_thread_t *bitmap_rt_pick_next(void)
{
	ulmk_arch_irq_key_t key = ulmk_arch_spin_lock_irqsave(&rq_lock);
	struct cpu_rq *q = rq_of_cpu((uint8_t)ulmk_arch_cpu_id());
	uint8_t        p;
	sys_dnode_t   *dn;
	ulmk_thread_t *t;

	p = bitmap_first_set(q);
	dn = sys_dlist_get(&q->level[p]);
	if (!dn) {
		ulmk_arch_spin_unlock_irqrestore(&rq_lock, key);
		return NULL;
	}

	if (sys_dlist_is_empty(&q->level[p]))
		bitmap_clear(q, p);

	t = SYS_DLIST_CONTAINER_OF(dn, ulmk_thread_t, sched_node);
	sys_dnode_init(&t->sched_node);
	ulmk_arch_spin_unlock_irqrestore(&rq_lock, key);
	return t;
}

static ulmk_thread_t *bitmap_rt_peek_next(void)
{
	ulmk_arch_irq_key_t key = ulmk_arch_spin_lock_irqsave(&rq_lock);
	struct cpu_rq *q = rq_of_cpu((uint8_t)ulmk_arch_cpu_id());
	uint8_t        p = bitmap_first_set(q);
	ulmk_thread_t *t;

	t = SYS_DLIST_PEEK_HEAD_CONTAINER_OF(&q->level[p], ulmk_thread_t,
					     sched_node);
	ulmk_arch_spin_unlock_irqrestore(&rq_lock, key);
	return t;
}

const ulmk_sched_class_t ulmk_bitmap_rt_class = {
	.name      = "bitmap-rt",
	.init      = bitmap_rt_init,
	.enqueue   = bitmap_rt_enqueue,
	.dequeue   = bitmap_rt_dequeue,
	.pick_next = bitmap_rt_pick_next,
	.peek_next = bitmap_rt_peek_next,
};
