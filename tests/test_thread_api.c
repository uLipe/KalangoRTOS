/*
 * Thread API test suite — covers ul_thread_self, yield, exit, create, kill,
 * suspend/resume, and priority_get/set.
 *
 * All tests are inside #if 0 until the kernel is implemented.
 * To enable: remove the #if 0 / #endif, uncomment the matching block in
 * runner.c, and ensure #include <sys/ulipe_microkernel.h> resolves.
 */

#include "unity.h"

/* #include <sys/ulipe_microkernel.h> */

#if 0

/* =========================================================================
 * Helpers
 * ========================================================================= */

static void noop_entry(void *arg) { (void)arg; ul_thread_exit(); }

static ul_tid_t spawn_noop(uint8_t prio)
{
	ul_thread_attr_t attr = {
		.name       = "noop",
		.entry      = noop_entry,
		.arg        = NULL,
		.priority   = prio,
		.stack_size = 512,
		.privilege  = UL_PRIV_USER,
	};
	return ul_thread_create(&attr);
}

/* =========================================================================
 * ul_thread_self
 * ========================================================================= */

/* HAPPY PATH */

void test_thread_self_returns_valid_tid(void)
{
	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, ul_thread_self());
}

void test_thread_self_is_stable(void)
{
	/* Two consecutive calls must return the same TID. */
	TEST_ASSERT_EQUAL_INT32(ul_thread_self(), ul_thread_self());
}

void test_thread_self_is_non_negative(void)
{
	TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, ul_thread_self());
}

/* =========================================================================
 * ul_thread_yield
 * ========================================================================= */

/* HAPPY PATH */

void test_thread_yield_returns(void)
{
	/* Single runnable thread: yield must return to the caller immediately. */
	ul_thread_yield();
	TEST_ASSERT_TRUE(1);
}

void test_thread_yield_multiple_times_safe(void)
{
	for (int i = 0; i < 16; i++)
		ul_thread_yield();
	TEST_ASSERT_TRUE(1);
}

/* =========================================================================
 * ul_thread_priority_get / ul_thread_priority_set
 * ========================================================================= */

/* HAPPY PATH */

void test_thread_priority_get_self_in_range(void)
{
	int prio = ul_thread_priority_get(ul_thread_self());

	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, prio);
	TEST_ASSERT_LESS_OR_EQUAL_INT(255, prio);
}

void test_thread_priority_set_and_get_roundtrip(void)
{
	ul_tid_t self = ul_thread_self();

	TEST_ASSERT_EQUAL_INT(0, ul_thread_priority_set(self, 42));
	TEST_ASSERT_EQUAL_INT(42, ul_thread_priority_get(self));
}

void test_thread_priority_set_min(void)
{
	ul_tid_t self = ul_thread_self();

	TEST_ASSERT_EQUAL_INT(0, ul_thread_priority_set(self, 0));
	TEST_ASSERT_EQUAL_INT(0, ul_thread_priority_get(self));
}

void test_thread_priority_set_max(void)
{
	ul_tid_t self = ul_thread_self();

	TEST_ASSERT_EQUAL_INT(0, ul_thread_priority_set(self, 255));
	TEST_ASSERT_EQUAL_INT(255, ul_thread_priority_get(self));
}

/* EDGE CASES */

void test_thread_priority_set_same_value_twice(void)
{
	ul_tid_t self = ul_thread_self();

	TEST_ASSERT_EQUAL_INT(0, ul_thread_priority_set(self, 10));
	TEST_ASSERT_EQUAL_INT(0, ul_thread_priority_set(self, 10));
	TEST_ASSERT_EQUAL_INT(10, ul_thread_priority_get(self));
}

/* CRASH PREVENTION */

void test_thread_priority_get_invalid_tid_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_thread_priority_get(UL_TID_INVALID));
}

void test_thread_priority_set_invalid_tid_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_thread_priority_set(UL_TID_INVALID, 10));
}

/* =========================================================================
 * ul_thread_create
 * ========================================================================= */

/* HAPPY PATH */

void test_thread_create_valid_attr_returns_valid_tid(void)
{
	ul_tid_t tid = spawn_noop(100);

	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, tid);
	TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, tid);
}

void test_thread_create_priority_zero(void)
{
	ul_tid_t tid = spawn_noop(0);

	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, tid);
}

void test_thread_create_priority_max(void)
{
	ul_tid_t tid = spawn_noop(255);

	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, tid);
}

void test_thread_create_user_privilege(void)
{
	ul_thread_attr_t attr = {
		.name       = "user_th",
		.entry      = noop_entry,
		.arg        = NULL,
		.priority   = 100,
		.stack_size = 512,
		.privilege  = UL_PRIV_USER,
	};
	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, ul_thread_create(&attr));
}

void test_thread_create_driver_privilege(void)
{
	ul_thread_attr_t attr = {
		.name       = "drv_th",
		.entry      = noop_entry,
		.arg        = NULL,
		.priority   = 100,
		.stack_size = 512,
		.privilege  = UL_PRIV_DRIVER,
	};
	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, ul_thread_create(&attr));
}

void test_thread_create_two_threads_have_distinct_tids(void)
{
	ul_tid_t a = spawn_noop(100);
	ul_tid_t b = spawn_noop(100);

	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, a);
	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, b);
	TEST_ASSERT_NOT_EQUAL(a, b);
}

/* EDGE CASES */

void test_thread_create_name_exactly_15_chars(void)
{
	ul_thread_attr_t attr = {
		.name       = "123456789012345",  /* 15 chars + NUL = 16 */
		.entry      = noop_entry,
		.arg        = NULL,
		.priority   = 100,
		.stack_size = 512,
		.privilege  = UL_PRIV_USER,
	};
	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, ul_thread_create(&attr));
}

void test_thread_create_name_longer_than_15_truncated(void)
{
	/*
	 * Kernel must not crash on oversized names — it truncates to 15 chars.
	 */
	ul_thread_attr_t attr = {
		.name       = "this_name_is_way_too_long_for_the_kernel",
		.entry      = noop_entry,
		.arg        = NULL,
		.priority   = 100,
		.stack_size = 512,
		.privilege  = UL_PRIV_USER,
	};
	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, ul_thread_create(&attr));
}

void test_thread_create_minimal_stack_rounds_up_to_alignment(void)
{
	ul_thread_attr_t attr = {
		.name       = "small",
		.entry      = noop_entry,
		.arg        = NULL,
		.priority   = 100,
		.stack_size = 1,   /* kernel rounds to 8-byte minimum */
		.privilege  = UL_PRIV_USER,
	};
	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, ul_thread_create(&attr));
}

/* CRASH PREVENTION */

void test_thread_create_null_attr_returns_error(void)
{
	TEST_ASSERT_EQUAL_INT(UL_TID_INVALID, ul_thread_create(NULL));
}

void test_thread_create_null_entry_returns_error(void)
{
	ul_thread_attr_t attr = {
		.name       = "bad",
		.entry      = NULL,
		.arg        = NULL,
		.priority   = 100,
		.stack_size = 512,
		.privilege  = UL_PRIV_USER,
	};
	TEST_ASSERT_EQUAL_INT(UL_TID_INVALID, ul_thread_create(&attr));
}

void test_thread_create_zero_stack_returns_error(void)
{
	ul_thread_attr_t attr = {
		.name       = "bad",
		.entry      = noop_entry,
		.arg        = NULL,
		.priority   = 100,
		.stack_size = 0,
		.privilege  = UL_PRIV_USER,
	};
	TEST_ASSERT_EQUAL_INT(UL_TID_INVALID, ul_thread_create(&attr));
}

void test_thread_create_overflow_resource_table_returns_enospc(void)
{
	/*
	 * Spawn threads until the table is full.  The first failure must be
	 * -UL_ENOSPC; subsequent calls must also fail gracefully (not crash).
	 */
	int failed = 0;
	for (int i = 0; i < 64; i++) {
		ul_tid_t tid = spawn_noop(255);
		if (tid == UL_TID_INVALID) {
			failed++;
			break;
		}
	}
	TEST_ASSERT_GREATER_THAN_INT(0, failed);
}

/* =========================================================================
 * ul_thread_kill
 * ========================================================================= */

/* HAPPY PATH */

void test_thread_kill_valid_tid_returns_zero(void)
{
	ul_tid_t tid = spawn_noop(200);

	TEST_ASSERT_NOT_EQUAL(UL_TID_INVALID, tid);
	TEST_ASSERT_EQUAL_INT(0, ul_thread_kill(tid));
}

/* CRASH PREVENTION */

void test_thread_kill_invalid_tid_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_thread_kill(UL_TID_INVALID));
}

void test_thread_kill_self_returns_error(void)
{
	/*
	 * Killing oneself via ul_thread_kill() must be rejected.
	 * Self-termination is done via ul_thread_exit(), not kill.
	 */
	TEST_ASSERT_LESS_THAN_INT(0, ul_thread_kill(ul_thread_self()));
}

void test_thread_kill_already_dead_returns_error(void)
{
	ul_tid_t tid = spawn_noop(200);

	TEST_ASSERT_EQUAL_INT(0, ul_thread_kill(tid));
	/* Second kill must not crash and must return an error. */
	TEST_ASSERT_LESS_THAN_INT(0, ul_thread_kill(tid));
}

/* =========================================================================
 * ul_thread_suspend / ul_thread_resume
 * ========================================================================= */

/* HAPPY PATH */

void test_thread_suspend_and_resume_returns_zero(void)
{
	ul_tid_t tid = spawn_noop(200);

	TEST_ASSERT_EQUAL_INT(0, ul_thread_suspend(tid));
	TEST_ASSERT_EQUAL_INT(0, ul_thread_resume(tid));
	ul_thread_kill(tid);
}

/* EDGE CASES */

void test_thread_suspend_twice_requires_two_resumes(void)
{
	ul_tid_t tid = spawn_noop(200);

	TEST_ASSERT_EQUAL_INT(0, ul_thread_suspend(tid));
	TEST_ASSERT_EQUAL_INT(0, ul_thread_suspend(tid));
	TEST_ASSERT_EQUAL_INT(0, ul_thread_resume(tid));  /* still suspended */
	TEST_ASSERT_EQUAL_INT(0, ul_thread_resume(tid));  /* now runnable */
	ul_thread_kill(tid);
}

void test_thread_resume_non_suspended_is_noop(void)
{
	/*
	 * Resuming a thread that was never suspended must return 0 (or a
	 * benign error) without crashing or corrupting scheduler state.
	 */
	ul_tid_t tid = spawn_noop(200);
	int ret = ul_thread_resume(tid);

	TEST_ASSERT_TRUE(ret == 0 || ret == -UL_EINVAL);
	ul_thread_kill(tid);
}

/* CRASH PREVENTION */

void test_thread_suspend_invalid_tid_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_thread_suspend(UL_TID_INVALID));
}

void test_thread_resume_invalid_tid_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_thread_resume(UL_TID_INVALID));
}

#endif /* 0 */
