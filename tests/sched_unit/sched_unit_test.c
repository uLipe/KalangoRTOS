/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Unit tests for the RT-FIFO scheduler — runs on host with mocked arch.
 *
 * Covers: priority ordering, FIFO within same priority, enqueue/dequeue,
 * yield re-insertion, empty-queue idle switch, self-switch guard.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <kernel/include/ulmk_sched.h>

/* =========================================================================
 * Arch stub — records context switches without executing any assembly
 * ========================================================================= */

int            switch_count;
ulmk_arch_ctx_t *last_from;
ulmk_arch_ctx_t *last_to;

void ulmk_arch_ctx_switch(ulmk_arch_ctx_t *from, ulmk_arch_ctx_t *to)
{
	switch_count++;
	last_from = from;
	last_to   = to;
}

void ulmk_arch_ctx_init(ulmk_arch_ctx_t *ctx,
		      void (*entry)(void *), void *arg,
		      uintptr_t stack_top, int priv)
{
	(void)entry; (void)arg; (void)stack_top; (void)priv;
	ctx->pcxi = 0;
}

void ulmk_arch_ctx_free(ulmk_arch_ctx_t *c) { (void)c; }
void ulmk_arch_mpu_switch(const ulmk_arch_region_t *r, uint8_t n, uint8_t p)
{
	(void)r; (void)n; (void)p;
}
void ulmk_thread_free(ulmk_thread_t *t) { (void)t; }

/* =========================================================================
 * Test harness
 * ========================================================================= */

static int tests_run;
static int tests_failed;

#define CHECK(cond, msg) \
	do { \
		tests_run++; \
		if (!(cond)) { \
			tests_failed++; \
			printf("  FAIL [line %d] %s\n", __LINE__, (msg)); \
		} else { \
			printf("  PASS %s\n", (msg)); \
		} \
	} while (0)

static void sched_reset(void)
{
	switch_count = 0;
	last_from    = NULL;
	last_to      = NULL;
	ulmk_sched_init();
	ulmk_sched_set_class(&ulmk_bitmap_rt_class);
}

static void make_thread(ulmk_thread_t *t, uint8_t prio)
{
	memset(t, 0, sizeof(*t));
	t->tid      = (ulmk_tid_t)(uintptr_t)t;
	t->priority = prio;
	t->state    = UL_THREAD_STATE_READY;
	sys_dnode_init(&t->sched_node);
	sys_dnode_init(&t->ipc_node);
	sys_dnode_init(&t->reg_node);
}

/* =========================================================================
 * Test cases
 * ========================================================================= */

static void test_empty_queue(void)
{
	printf("\n[test_empty_queue]\n");
	sched_reset();
	CHECK(ulmk_sched_peek_next() == NULL, "peek_next on empty queue → NULL");
}

static void test_single_enqueue(void)
{
	ulmk_thread_t t;

	printf("\n[test_single_enqueue]\n");
	sched_reset();
	make_thread(&t, 5);
	ulmk_sched_enqueue(&t);
	CHECK(ulmk_sched_peek_next() == &t, "single thread picked");
	CHECK(t.state == UL_THREAD_STATE_READY, "enqueue sets READY");
}

static void test_priority_order(void)
{
	ulmk_thread_t lo, hi, mid;

	printf("\n[test_priority_order]\n");
	sched_reset();
	make_thread(&lo,  10);
	make_thread(&hi,  1);
	make_thread(&mid, 5);

	ulmk_sched_enqueue(&lo);
	ulmk_sched_enqueue(&hi);
	ulmk_sched_enqueue(&mid);

	CHECK(ulmk_sched_peek_next() == &hi, "prio=1 picked over prio=5 and prio=10");
}

static void test_fifo_within_priority(void)
{
	ulmk_thread_t a, b, c;

	printf("\n[test_fifo_within_priority]\n");
	sched_reset();
	make_thread(&a, 3);
	make_thread(&b, 3);
	make_thread(&c, 3);

	ulmk_sched_enqueue(&a);
	ulmk_sched_enqueue(&b);
	ulmk_sched_enqueue(&c);

	CHECK(ulmk_sched_peek_next() == &a,
	      "first enqueued is head of same-priority group");
}

static void test_dequeue(void)
{
	ulmk_thread_t a, b;

	printf("\n[test_dequeue]\n");
	sched_reset();
	make_thread(&a, 1);
	make_thread(&b, 2);

	ulmk_sched_enqueue(&a);
	ulmk_sched_enqueue(&b);
	ulmk_sched_dequeue(&a);

	CHECK(ulmk_sched_peek_next() == &b, "dequeued thread no longer picked");
	CHECK(!sys_dnode_is_linked(&a.sched_node), "dequeued thread node cleared");
}

static void test_dequeue_not_enqueued(void)
{
	ulmk_thread_t a, b;

	printf("\n[test_dequeue_not_enqueued]\n");
	sched_reset();
	make_thread(&a, 2);
	make_thread(&b, 3);

	ulmk_sched_enqueue(&b);
	ulmk_sched_dequeue(&a); /* a never enqueued — must not corrupt */

	CHECK(ulmk_sched_peek_next() == &b, "queue intact after spurious dequeue");
}

static void test_yield_reorder(void)
{
	ulmk_thread_t a, b;

	printf("\n[test_yield_reorder]\n");
	sched_reset();
	make_thread(&a, 5);
	make_thread(&b, 5);

	ulmk_sched_enqueue(&a);
	ulmk_sched_enqueue(&b);

	ulmk_sched_dequeue(&a);
	ulmk_sched_enqueue(&a);

	CHECK(ulmk_sched_peek_next() == &b,
	      "after yield, peer at same priority becomes head");
}

static void test_sched_start(void)
{
	ulmk_thread_t a, idle;

	printf("\n[test_sched_start]\n");
	sched_reset();
	make_thread(&a,    3);
	make_thread(&idle, 255);

	ulmk_sched_enqueue(&idle);
	ulmk_sched_enqueue(&a);
	ulmk_sched_start();

	CHECK(switch_count == 1,  "ulmk_sched_start triggers one switch");
	CHECK(last_to == &a.ctx,  "switched TO thread a");
	CHECK(ulmk_sched_current() == &a,             "sched_current = a after start");
	CHECK(a.state == UL_THREAD_STATE_RUNNING,   "thread state = RUNNING");
}

static void test_schedule_to_idle_on_empty(void)
{
	ulmk_thread_t a, idle;

	printf("\n[test_schedule_to_idle_on_empty]\n");
	sched_reset();
	make_thread(&a,    3);
	make_thread(&idle, 255);

	ulmk_sched_enqueue(&idle);
	ulmk_sched_enqueue(&a);
	ulmk_sched_start();
	switch_count = 0;

	/* a exits: dequeue and schedule → idle is the only remaining thread */
	a.state = UL_THREAD_STATE_DEAD;
	ulmk_sched_dequeue(&a);
	ulmk_sched_resched();

	CHECK(switch_count == 1,         "schedule with no work → one switch");
	CHECK(last_to == &idle.ctx,      "switch to idle thread");
	CHECK(ulmk_sched_current() == &idle, "sched_current = idle");
}

static void test_self_switch_guard(void)
{
	ulmk_thread_t a, idle;

	printf("\n[test_self_switch_guard]\n");
	sched_reset();
	make_thread(&a,    5);
	make_thread(&idle, 255);

	ulmk_sched_enqueue(&idle);
	ulmk_sched_enqueue(&a);
	ulmk_sched_start();
	switch_count = 0;

	/* Yield of only thread at its priority: from == to → no switch */
	a.state = UL_THREAD_STATE_READY;
	ulmk_sched_dequeue(&a);
	ulmk_sched_enqueue(&a);
	ulmk_sched_resched();

	CHECK(switch_count == 0,
	      "from==to guard skips switch when only one thread");
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
	printf("=== sched unit tests ===\n");

	test_empty_queue();
	test_single_enqueue();
	test_priority_order();
	test_fifo_within_priority();
	test_dequeue();
	test_dequeue_not_enqueued();
	test_yield_reorder();
	test_sched_start();
	test_schedule_to_idle_on_empty();
	test_self_switch_guard();

	printf("\n=== results: %d/%d passed ===\n",
	       tests_run - tests_failed, tests_run);

	return tests_failed ? 1 : 0;
}
