/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * sleep_unit_test.c — host unit tests for kernel/timer/timer.c
 *
 * Tests the two-primitive timer model:
 *   ul_timer_set_deadline(us) — programs a relative deadline in µs.
 *   ul_timer_wait_thread(cur) — blocks a thread; ul_timer_tick() wakes it.
 *   ul_timer_waiter_cancel(th) — cancels the wait before the deadline.
 *
 * Test plan:
 *   1. Monotonic time advances one TICK_PERIOD_US per ul_timer_tick().
 *   2. Timer wakes waiter when deadline expires.
 *   3. Timer does not wake waiter before deadline.
 *   4. ul_timer_waiter_cancel() removes the waiter; tick no longer wakes it.
 *   5. A second set_deadline overwrites the first.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ── Stubs ────────────────────────────────────────────────────────────────── */

#include "include/ul_arch.h"
#include "include/ul/microkernel.h"
#include "include/ul/config.h"
#include "../../kernel/include/ul_thread_internal.h"
#include "../../kernel/include/ul_sched.h"
#include "../../kernel/include/ul_timer_internal.h"

void ul_arch_ctx_switch(ul_arch_ctx_t *f, ul_arch_ctx_t *t) { (void)f; (void)t; }
void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *), void *arg,
		      uintptr_t sp, int priv)
{
	(void)ctx; (void)entry; (void)arg; (void)sp; (void)priv;
}
void ul_arch_ctx_free(ul_arch_ctx_t *ctx) { (void)ctx; }
void ul_arch_mpu_switch(const ul_arch_region_t *r, uint8_t c, uint8_t p)
{
	(void)r; (void)c; (void)p;
}

static ul_thread_t *g_current;
static ul_thread_t *g_last_enqueued;
static int          g_schedule_calls;

ul_thread_t *ul_sched_current(void)          { return g_current; }
void         ul_sched_schedule(void)          { g_schedule_calls++; }
void         ul_sched_dequeue(ul_thread_t *t) { (void)t; }
void         ul_sched_enqueue(ul_thread_t *t)
{
	t->state        = UL_THREAD_STATE_READY;
	g_last_enqueued = t;
}
void         ul_sched_init(void)                           {}
void         ul_sched_set_class(const ul_sched_class_t *c) { (void)c; }
void         ul_sched_start(void)                          {}
ul_thread_t *ul_sched_peek_next(void)                      { return NULL; }
void         ul_sched_tick(void)                           {}

/* ── Helpers ──────────────────────────────────────────────────────────────── */

#define TICK_PERIOD_US	(1000000u / UL_CONFIG_TICK_HZ)

#define PASS(name) printf("  [PASS] %s\n", name)
#define FAIL(name, msg) do { \
	printf("  [FAIL] %s: %s\n", name, msg); \
	failures++; \
} while (0)

static int failures;

static void reset_stubs(void)
{
	g_current        = NULL;
	g_last_enqueued  = NULL;
	g_schedule_calls = 0;
}

static ul_thread_t make_thread(uint8_t prio)
{
	ul_thread_t t;
	memset(&t, 0, sizeof(t));
	t.priority = prio;
	t.state    = UL_THREAD_STATE_BLOCKED;
	return t;
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

/*
 * Each ul_timer_tick() call must advance ul_timer_now_us() by exactly
 * TICK_PERIOD_US microseconds.
 */
static void test_monotonic_advance(void)
{
	const char *name = "monotonic_advance";
	uint64_t t0, t1, t2;

	reset_stubs();
	ul_timer_init();

	t0 = ul_timer_now_us();
	ul_timer_tick();
	t1 = ul_timer_now_us();
	ul_timer_tick();
	ul_timer_tick();
	t2 = ul_timer_now_us();

	if (t1 <= t0)
		FAIL(name, "time did not advance after one tick");
	else if (t1 - t0 != TICK_PERIOD_US)
		FAIL(name, "wrong delta for one tick");
	else if (t2 - t0 != 3u * TICK_PERIOD_US)
		FAIL(name, "wrong delta for three ticks");
	else
		PASS(name);
}

/*
 * The waiter must be enqueued when the deadline expires.
 */
static void test_waiter_woken_on_deadline(void)
{
	const char *name = "waiter_woken_on_deadline";
	ul_thread_t ta;

	reset_stubs();
	ul_timer_init();

	ta = make_thread(0);
	g_current = &ta;

	ul_timer_set_deadline(TICK_PERIOD_US);		/* 1 tick from now */
	ul_timer_waiter_cancel(NULL);			/* no-op; dummy call */

	/*
	 * Simulate what ul_timer_wait_thread() does without the actual
	 * context switch: register the waiter manually.
	 */
	ta.state          = UL_THREAD_STATE_BLOCKED;
	ta.blocked_reason = UL_BLOCKED_TIMER_WAIT;
	/* Directly install the waiter via the public cancel + re-arm path
	 * by calling wait_thread. Because our ul_sched_schedule stub does
	 * not actually switch context, the call returns immediately. */
	ul_timer_wait_thread(&ta);

	/* After the fake "return" from wait_thread (stub schedule is no-op),
	 * tick once to expire the deadline.  The waiter should be enqueued. */
	ul_timer_tick();

	if (g_last_enqueued != &ta)
		FAIL(name, "waiter not enqueued after deadline tick");
	else
		PASS(name);
}

/*
 * ul_timer_tick() must not wake the waiter before the deadline expires.
 */
static void test_no_early_wake(void)
{
	const char *name = "no_early_wake";
	ul_thread_t ta;

	reset_stubs();
	ul_timer_init();

	ta = make_thread(0);
	ta.blocked_reason = UL_BLOCKED_TIMER_WAIT;
	ul_timer_set_deadline(3u * TICK_PERIOD_US);
	ul_timer_wait_thread(&ta);

	/* Two ticks — deadline is at 3 ticks; waiter must not wake yet. */
	ul_timer_tick();
	if (g_last_enqueued == &ta) {
		FAIL(name, "waiter woken on tick 1 (too early)");
		return;
	}
	ul_timer_tick();
	if (g_last_enqueued == &ta) {
		FAIL(name, "waiter woken on tick 2 (too early)");
		return;
	}

	/* Third tick — now it should fire. */
	ul_timer_tick();
	if (g_last_enqueued != &ta)
		FAIL(name, "waiter not woken on deadline tick");
	else
		PASS(name);
}

/*
 * ul_timer_waiter_cancel() must remove the waiter; subsequent ticks
 * must not enqueue it.
 */
static void test_cancel_prevents_wake(void)
{
	const char *name = "cancel_prevents_wake";
	ul_thread_t ta;

	reset_stubs();
	ul_timer_init();

	ta = make_thread(0);
	ta.blocked_reason = UL_BLOCKED_TIMER_WAIT;
	ul_timer_set_deadline(TICK_PERIOD_US);
	ul_timer_wait_thread(&ta);

	ul_timer_waiter_cancel(&ta);

	ul_timer_tick();	/* would have been the deadline tick */

	if (g_last_enqueued == &ta)
		FAIL(name, "cancelled waiter was enqueued");
	else
		PASS(name);
}

/*
 * A second ul_timer_set_deadline() call must overwrite the first;
 * the waiter must fire at the new deadline.
 */
static void test_overwrite_deadline(void)
{
	const char *name = "overwrite_deadline";
	ul_thread_t ta;

	reset_stubs();
	ul_timer_init();

	ta = make_thread(0);
	ta.blocked_reason = UL_BLOCKED_TIMER_WAIT;

	ul_timer_set_deadline(TICK_PERIOD_US);		/* first: 1 tick */
	ul_timer_set_deadline(3u * TICK_PERIOD_US);	/* override: 3 ticks */
	ul_timer_wait_thread(&ta);

	ul_timer_tick();	/* tick 1 — old deadline; must NOT fire */
	if (g_last_enqueued == &ta) {
		FAIL(name, "fired at overwritten deadline");
		return;
	}
	ul_timer_tick();	/* tick 2 — still early */
	ul_timer_tick();	/* tick 3 — new deadline */

	if (g_last_enqueued != &ta)
		FAIL(name, "waiter not fired at new deadline");
	else
		PASS(name);
}

/* ── main ──────────────────────────────────────────────────────────────────── */

int main(void)
{
	printf("sleep unit tests\n");

	test_monotonic_advance();
	test_waiter_woken_on_deadline();
	test_no_early_wake();
	test_cancel_prevents_wake();
	test_overwrite_deadline();

	if (failures) {
		printf("\n%d test(s) FAILED\n", failures);
		return 1;
	}
	printf("\nAll tests PASSED\n");
	return 0;
}
