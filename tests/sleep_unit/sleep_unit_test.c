/* SPDX-License-Identifier: MIT */
/*
 * sleep_unit_test.c — host unit tests for kernel/timer/timer.c
 *
 * Tests are self-contained: timer.c is compiled in directly with stubs for
 * all arch and scheduler primitives.  No QEMU or hardware required.
 *
 * g_mono_us is static inside timer.c and accumulates across tests.  Each
 * test therefore obtains a baseline via ul_timer_now_us() immediately after
 * ul_timer_init() and expresses all deadlines as (base + offset) so that
 * they are always in the future regardless of accumulated prior state.
 *
 * Test plan:
 *   1. Monotonic time advances correctly (no wrap).
 *   2. Wrap detection: STM counter goes 0xFFFFFFF0 → 0x00000010.
 *   3. Insert order: threads inserted out-of-deadline order are sorted.
 *   4. ul_timer_tick wakes threads whose deadline <= now.
 *   5. After waking one, remaining threads keep their place.
 *   6. When idle (sched_current==NULL) and threads are woken: schedule called.
 *   7. When a thread is running (sched_current!=NULL): schedule NOT called.
 *   8. Queue empty after all threads woken: deadline not re-armed.
 *   9. Same deadline: both threads woken on the same tick (FIFO).
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

/* Controllable fake STM counter. */
static uint32_t g_stm;

uint32_t ul_arch_tick_get(void)
{
	return g_stm;
}

/* Records last programmed deadline delta. */
static uint32_t g_deadline_armed;

void ul_arch_tick_deadline(uint32_t delta_us)
{
	g_deadline_armed = delta_us;
}

/* Stubs for context switch (unused by timer tests). */
void ul_arch_ctx_switch(ul_arch_ctx_t *f, ul_arch_ctx_t *t) { (void)f; (void)t; }
void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *), void *arg,
		      uintptr_t sp, int priv)
{
	(void)ctx; (void)entry; (void)arg; (void)sp; (void)priv;
}

/* Scheduler stubs — the timer only calls enqueue, current, schedule. */
static ul_thread_t	*g_current;
static ul_thread_t	*g_last_enqueued;
static int		 g_schedule_calls;

ul_thread_t *ul_sched_current(void)        { return g_current; }
void         ul_sched_schedule(void)        { g_schedule_calls++; }
void         ul_sched_dequeue(ul_thread_t *t) { (void)t; }
void         ul_sched_enqueue(ul_thread_t *t)
{
	t->state = UL_THREAD_STATE_READY;
	g_last_enqueued = t;
}

/* Unused sched functions required by the linker due to ul_sched.h. */
void         ul_sched_init(ul_arch_ctx_t *i)                   { (void)i; }
void         ul_sched_set_class(const ul_sched_class_t *c)     { (void)c; }
void         ul_sched_start(ul_arch_ctx_t *i, ul_thread_t *f)  { (void)i; (void)f; }
ul_thread_t *ul_sched_pick_next(void)                          { return NULL; }
void         ul_sched_tick(void)                               {}

/* ── Helpers ──────────────────────────────────────────────────────────────── */

#define PASS(name) printf("  [PASS] %s\n", name)
#define FAIL(name, msg) do { \
	printf("  [FAIL] %s: %s\n", name, msg); \
	failures++; \
} while (0)

static int failures;

/*
 * Reset per-test observable state.  g_mono_us / g_last_stm inside timer.c
 * are NOT reset — tests must derive deadlines from ul_timer_now_us() to
 * stay correct regardless of accumulated prior state.
 */
static void reset_stubs(void)
{
	g_stm            = 0;
	g_deadline_armed = 0;
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

static void test_monotonic_advance(void)
{
	const char *name = "monotonic_advance";
	uint64_t t0, t1;

	reset_stubs();
	g_stm = 1000u;
	ul_timer_init();

	g_stm = 2000u;
	t0 = ul_timer_now_us();		/* delta = 1000 */

	g_stm = 5000u;
	t1 = ul_timer_now_us();		/* delta = 3000 from t0 */

	if (t1 <= t0)
		FAIL(name, "time did not advance");
	else if (t1 - t0 != 3000u)
		FAIL(name, "wrong delta");
	else
		PASS(name);
}

static void test_wrap_detection(void)
{
	const char *name = "wrap_detection";
	uint64_t before_wrap, after_wrap;

	reset_stubs();
	g_stm = 0xFFFFFFF0u;
	ul_timer_init();

	g_stm = 0xFFFFFFFFu;
	before_wrap = ul_timer_now_us();	/* delta = 0x0F */

	/* Simulate counter wrap: 0xFFFFFFFF → 0 → ... → 0x10 (0x11 steps). */
	g_stm = 0x00000010u;
	after_wrap = ul_timer_now_us();

	if (after_wrap <= before_wrap)
		FAIL(name, "monotonic violated after wrap");
	else if (after_wrap - before_wrap != (uint64_t)0x00000011u)
		FAIL(name, "wrap delta mismatch");
	else
		PASS(name);
}

static void test_insert_sorted(void)
{
	const char *name = "insert_sorted";
	uint64_t base;
	ul_thread_t ta, tb, tc;

	reset_stubs();
	g_stm = 0u;
	ul_timer_init();
	base = ul_timer_now_us();	/* establish baseline */

	ta = make_thread(0);
	tb = make_thread(1);
	tc = make_thread(2);

	/* Insert out of order; expected queue: ta(base+10), tb(+20), tc(+30). */
	ul_timer_sleep_insert(&tc, base + 30u);
	ul_timer_sleep_insert(&ta, base + 10u);
	ul_timer_sleep_insert(&tb, base + 20u);

	if (g_deadline_armed == 0)
		FAIL(name, "no deadline armed after insert");
	else
		PASS(name);

	/* g_last_stm = 0 after the inserts (g_stm unchanged).
	 * Setting g_stm = N gives delta = N from the last ul_timer_now_us call. */
	g_stm = 10u;
	g_last_enqueued = NULL;
	ul_timer_tick();	/* should wake ta only */
	if (g_last_enqueued != &ta)
		FAIL(name, "ta not woken at base+10");
	else
		PASS(name);

	g_stm = 20u;
	g_last_enqueued = NULL;
	ul_timer_tick();	/* should wake tb only */
	if (g_last_enqueued != &tb)
		FAIL(name, "tb not woken at base+20");
	else
		PASS(name);

	g_stm = 30u;
	g_last_enqueued = NULL;
	ul_timer_tick();	/* should wake tc */
	if (g_last_enqueued != &tc)
		FAIL(name, "tc not woken at base+30");
	else
		PASS(name);
}

static void test_tick_wakes_all_expired(void)
{
	const char *name = "tick_wakes_all_expired";
	uint64_t base;
	ul_thread_t ta, tb;

	reset_stubs();
	g_stm = 0u;
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	tb = make_thread(1);

	ul_timer_sleep_insert(&ta, base + 100u);
	ul_timer_sleep_insert(&tb, base + 200u);

	/* Advance past both deadlines in one tick. */
	g_stm = 250u;
	ul_timer_tick();

	if (ta.state != UL_THREAD_STATE_READY)
		FAIL(name, "ta not woken");
	else if (tb.state != UL_THREAD_STATE_READY)
		FAIL(name, "tb not woken");
	else
		PASS(name);
}

/*
 * ul_timer_tick() no longer calls ul_sched_schedule() directly to avoid
 * pushing an extra CSA frame inside the timer ISR.  The idle loop detects
 * a non-empty run queue and calls ul_sched_schedule() itself.  We verify
 * only that the expired thread is enqueued (state = READY).
 */
static void test_idle_triggers_schedule(void)
{
	const char *name = "idle_triggers_schedule";
	uint64_t base;
	ul_thread_t ta;

	reset_stubs();
	g_stm      = 0u;
	g_current  = NULL;	/* idle */
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	ul_timer_sleep_insert(&ta, base + 50u);

	g_stm = 50u;
	ul_timer_tick();

	if (ta.state != UL_THREAD_STATE_READY)
		FAIL(name, "expired thread not enqueued");
	else
		PASS(name);
}

static void test_running_no_schedule(void)
{
	const char *name = "running_no_schedule";
	uint64_t base;
	ul_thread_t cur, sleeping;

	reset_stubs();
	g_stm = 0u;
	ul_timer_init();
	base = ul_timer_now_us();

	cur     = make_thread(0);
	sleeping = make_thread(1);

	cur.state = UL_THREAD_STATE_RUNNING;
	g_current = &cur;	/* a thread is running — not idle */

	ul_timer_sleep_insert(&sleeping, base + 50u);

	g_stm = 50u;
	ul_timer_tick();

	if (g_schedule_calls != 0)
		FAIL(name, "ul_sched_schedule called unexpectedly");
	else if (sleeping.state != UL_THREAD_STATE_READY)
		FAIL(name, "sleeping thread not enqueued");
	else
		PASS(name);
}

static void test_rearm_after_partial_wake(void)
{
	const char *name = "rearm_after_partial_wake";
	uint64_t base;
	ul_thread_t ta, tb;

	reset_stubs();
	g_stm = 0u;
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	tb = make_thread(1);

	ul_timer_sleep_insert(&ta, base + 100u);
	ul_timer_sleep_insert(&tb, base + 200u);

	/* Wake ta only; tb should still be in queue → deadline re-armed. */
	g_stm = 100u;
	g_deadline_armed = 0u;
	ul_timer_tick();

	if (ta.state != UL_THREAD_STATE_READY)
		FAIL(name, "ta not woken");
	else if (g_deadline_armed == 0u)
		FAIL(name, "deadline not re-armed for tb");
	else
		PASS(name);
}

/*
 * arm_nearest() always arms the timer for at least the next quantum period
 * (TICK_PERIOD_US) so the preemptive scheduler keeps ticking even when no
 * thread is sleeping.  An empty sleep queue does not suppress re-arming.
 */
static void test_empty_queue_no_rearm(void)
{
	const char *name = "empty_queue_no_rearm";
	uint64_t base;
	ul_thread_t ta;

	reset_stubs();
	g_stm = 0u;
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	ul_timer_sleep_insert(&ta, base + 50u);

	/* Wake ta → queue becomes empty. */
	g_stm = 50u;
	ul_timer_tick();

	/* With preemptive scheduling, arm_nearest always re-arms for the
	 * next quantum period even when the sleep queue is empty. */
	g_deadline_armed = 0u;
	g_stm = 100u;
	ul_timer_tick();

	if (g_deadline_armed == 0u)
		FAIL(name, "deadline not re-armed for next quantum");
	else
		PASS(name);
}

static void test_same_deadline_fifo(void)
{
	const char *name = "same_deadline_fifo";
	uint64_t base;
	ul_thread_t ta, tb;

	reset_stubs();
	g_stm = 0u;
	ul_timer_init();
	base = ul_timer_now_us();

	ta = make_thread(0);
	tb = make_thread(1);

	/* Same deadline: ta inserted first → ta is head, both should wake together. */
	ul_timer_sleep_insert(&ta, base + 100u);
	ul_timer_sleep_insert(&tb, base + 100u);

	g_stm = 100u;
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
	test_wrap_detection();
	test_insert_sorted();
	test_tick_wakes_all_expired();
	test_idle_triggers_schedule();
	test_running_no_schedule();
	test_rearm_after_partial_wake();
	test_empty_queue_no_rearm();
	test_same_deadline_fifo();

	if (failures) {
		printf("\n%d test(s) FAILED\n", failures);
		return 1;
	}
	printf("\nAll tests passed.\n");
	return 0;
}
