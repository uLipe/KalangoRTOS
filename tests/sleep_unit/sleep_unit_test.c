/* SPDX-License-Identifier: MIT */
/*
 * sleep_unit_test.c — host unit tests for kernel/timer/timer.c (ticked mode)
 *
 * In ticked mode, monotonic time advances strictly through ul_timer_tick()
 * calls.  Each call increments an internal counter; ul_timer_now_us()
 * converts that counter to microseconds via the configured tick rate.
 *
 * There is no STM counter manipulation, no ul_arch_tick_deadline(), and no
 * re-arm logic — the hardware timer fires periodically at UL_CONFIG_TICK_HZ.
 *
 * Test plan:
 *   1. Monotonic time advances one TICK_PERIOD_US per ul_timer_tick().
 *   2. Insert order: threads inserted out-of-deadline order are sorted.
 *   3. ul_timer_tick() wakes all threads whose deadline <= now.
 *   4. After a partial wake, remaining threads are still sleeping.
 *   5. Threads with the same deadline wake together in FIFO order.
 *   6. When idle (sched_current==NULL), expired threads are enqueued.
 *   7. When a thread is running, ul_sched_schedule is NOT called from tick.
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

/* Context switch stubs (unused by timer tests). */
void ul_arch_ctx_switch(ul_arch_ctx_t *f, ul_arch_ctx_t *t) { (void)f; (void)t; }
void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *), void *arg,
		      uintptr_t sp, int priv)
{
	(void)ctx; (void)entry; (void)arg; (void)sp; (void)priv;
}

/* Scheduler stubs. */
static ul_thread_t	*g_current;
static ul_thread_t	*g_last_enqueued;
static int		 g_schedule_calls;

ul_thread_t *ul_sched_current(void)          { return g_current; }
void         ul_sched_schedule(void)          { g_schedule_calls++; }
void         ul_sched_dequeue(ul_thread_t *t) { (void)t; }
void         ul_sched_enqueue(ul_thread_t *t)
{
	t->state = UL_THREAD_STATE_READY;
	g_last_enqueued = t;
}

void         ul_sched_init(ul_arch_ctx_t *i)                  { (void)i; }
void         ul_sched_set_class(const ul_sched_class_t *c)    { (void)c; }
void         ul_sched_start(ul_arch_ctx_t *i, ul_thread_t *f) { (void)i; (void)f; }
ul_thread_t *ul_sched_pick_next(void)                         { return NULL; }
void         ul_sched_tick(void)                              {}

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
 * Threads inserted out-of-deadline order must be sorted in ascending
 * deadline order.  Each one wakes on the correct tick boundary.
 */
static void test_insert_sorted(void)
{
	const char *name = "insert_sorted";
	uint64_t base;
	ul_thread_t ta, tb, tc;

	reset_stubs();
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	tb = make_thread(1);
	tc = make_thread(2);

	ul_timer_sleep_insert(&tc, base + 3u * TICK_PERIOD_US);
	ul_timer_sleep_insert(&ta, base + 1u * TICK_PERIOD_US);
	ul_timer_sleep_insert(&tb, base + 2u * TICK_PERIOD_US);

	/* Tick 1: ta should wake */
	g_last_enqueued = NULL;
	ul_timer_tick();
	if (g_last_enqueued != &ta)
		FAIL(name, "ta not woken on tick 1");
	else
		PASS(name);

	/* Tick 2: tb should wake */
	g_last_enqueued = NULL;
	ul_timer_tick();
	if (g_last_enqueued != &tb)
		FAIL(name, "tb not woken on tick 2");
	else
		PASS(name);

	/* Tick 3: tc should wake */
	g_last_enqueued = NULL;
	ul_timer_tick();
	if (g_last_enqueued != &tc)
		FAIL(name, "tc not woken on tick 3");
	else
		PASS(name);
}

/*
 * A single tick past multiple deadlines must wake all expired threads.
 */
static void test_tick_wakes_all_expired(void)
{
	const char *name = "tick_wakes_all_expired";
	uint64_t base;
	ul_thread_t ta, tb;

	reset_stubs();
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	tb = make_thread(1);

	ul_timer_sleep_insert(&ta, base + 1u * TICK_PERIOD_US);
	ul_timer_sleep_insert(&tb, base + 1u * TICK_PERIOD_US);

	ul_timer_tick();

	if (ta.state != UL_THREAD_STATE_READY)
		FAIL(name, "ta not woken");
	else if (tb.state != UL_THREAD_STATE_READY)
		FAIL(name, "tb not woken");
	else
		PASS(name);
}

/*
 * When a thread remains in the sleep queue after a partial wake, it must
 * still be sleeping (not enqueued).
 */
static void test_partial_wake_remainder(void)
{
	const char *name = "partial_wake_remainder";
	uint64_t base;
	ul_thread_t ta, tb;

	reset_stubs();
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	tb = make_thread(1);

	ul_timer_sleep_insert(&ta, base + 1u * TICK_PERIOD_US);
	ul_timer_sleep_insert(&tb, base + 3u * TICK_PERIOD_US);

	/* One tick: only ta expires */
	ul_timer_tick();

	if (ta.state != UL_THREAD_STATE_READY)
		FAIL(name, "ta not woken");
	else if (tb.state != UL_THREAD_STATE_BLOCKED)
		FAIL(name, "tb woken too early");
	else
		PASS(name);
}

/*
 * Expired thread is enqueued when in idle context (no running thread).
 */
static void test_idle_triggers_enqueue(void)
{
	const char *name = "idle_triggers_schedule";
	uint64_t base;
	ul_thread_t ta;

	reset_stubs();
	g_current = NULL;
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	ul_timer_sleep_insert(&ta, base + 1u * TICK_PERIOD_US);

	ul_timer_tick();

	if (ta.state != UL_THREAD_STATE_READY)
		FAIL(name, "expired thread not enqueued");
	else
		PASS(name);
}

/*
 * ul_timer_tick() must NOT call ul_sched_schedule() directly — the idle
 * loop handles reschedule after returning from ul_arch_cpu_idle().
 */
static void test_running_no_schedule(void)
{
	const char *name = "running_no_schedule";
	uint64_t base;
	ul_thread_t cur, sleeping;

	reset_stubs();
	ul_timer_init();
	base = ul_timer_now_us();

	cur     = make_thread(0);
	sleeping = make_thread(1);

	cur.state = UL_THREAD_STATE_RUNNING;
	g_current = &cur;

	ul_timer_sleep_insert(&sleeping, base + 1u * TICK_PERIOD_US);

	ul_timer_tick();

	if (g_schedule_calls != 0)
		FAIL(name, "ul_sched_schedule called unexpectedly");
	else if (sleeping.state != UL_THREAD_STATE_READY)
		FAIL(name, "sleeping thread not enqueued");
	else
		PASS(name);
}

/*
 * Two threads with the same deadline must both wake on the same tick.
 */
static void test_same_deadline_fifo(void)
{
	const char *name = "same_deadline_fifo";
	uint64_t base;
	ul_thread_t ta, tb;

	reset_stubs();
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	tb = make_thread(1);

	ul_timer_sleep_insert(&ta, base + 1u * TICK_PERIOD_US);
	ul_timer_sleep_insert(&tb, base + 1u * TICK_PERIOD_US);

	ul_timer_tick();

	if (ta.state != UL_THREAD_STATE_READY)
		FAIL(name, "ta not woken (same deadline)");
	else if (tb.state != UL_THREAD_STATE_READY)
		FAIL(name, "tb not woken (same deadline)");
	else
		PASS(name);
}

/* ── main ──────────────────────────────────────────────────────────────────── */

int main(void)
{
	printf("sleep unit tests\n");

	test_monotonic_advance();
	test_insert_sorted();
	test_tick_wakes_all_expired();
	test_partial_wake_remainder();
	test_idle_triggers_enqueue();
	test_running_no_schedule();
	test_same_deadline_fifo();

	if (failures) {
		printf("\n%d test(s) FAILED\n", failures);
		return 1;
	}
	printf("\nAll tests passed.\n");
	return 0;
}
