/*
 * IPC Endpoint API test suite — covers ul_ep_create, call, recv, reply,
 * reply_recv, and grant.
 *
 * NOTE: ul_ep_call / ul_ep_recv / ul_ep_reply require two threads executing
 * concurrently.  Those interactions belong in the integration test suite.
 * Tests here verify single-threaded observable behaviour: handle validity,
 * error returns, and structural invariants of ul_msg_t.
 *
 * All tests are inside #if 0 until the kernel is implemented.
 */

#include "unity.h"

/* #include <sys/ulipe_microkernel.h> */

#if 0

/* =========================================================================
 * ul_ep_create
 * ========================================================================= */

/* HAPPY PATH */

void test_ep_create_returns_valid_handle(void)
{
	ul_ep_t ep = ul_ep_create();

	TEST_ASSERT_NOT_EQUAL(UL_EP_INVALID, ep);
	TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, ep);
}

void test_ep_create_multiple_returns_distinct_handles(void)
{
	ul_ep_t a = ul_ep_create();
	ul_ep_t b = ul_ep_create();

	TEST_ASSERT_NOT_EQUAL(UL_EP_INVALID, a);
	TEST_ASSERT_NOT_EQUAL(UL_EP_INVALID, b);
	TEST_ASSERT_NOT_EQUAL(a, b);
}

void test_ep_create_three_handles_all_distinct(void)
{
	ul_ep_t a = ul_ep_create();
	ul_ep_t b = ul_ep_create();
	ul_ep_t c = ul_ep_create();

	TEST_ASSERT_NOT_EQUAL(a, b);
	TEST_ASSERT_NOT_EQUAL(b, c);
	TEST_ASSERT_NOT_EQUAL(a, c);
}

/* EDGE CASES */

void test_ep_create_up_to_system_limit_returns_enospc(void)
{
	/*
	 * Create endpoints until the table overflows.  The first refusal must
	 * be negative and must not crash the kernel.
	 */
	int failed = 0;
	for (int i = 0; i < 256; i++) {
		ul_ep_t ep = ul_ep_create();
		if (ep == UL_EP_INVALID) {
			failed++;
			break;
		}
	}
	TEST_ASSERT_GREATER_THAN_INT(0, failed);
}

/* =========================================================================
 * ul_msg_t structural invariants
 * ========================================================================= */

void test_msg_label_zero_is_valid(void)
{
	ul_msg_t msg = { .label = 0 };

	TEST_ASSERT_EQUAL_UINT32(0u, msg.label);
}

void test_msg_label_max_is_valid(void)
{
	ul_msg_t msg = { .label = 0xFFFFFFFFu };

	TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, msg.label);
}

void test_msg_words_are_independently_addressable(void)
{
	ul_msg_t msg;
	uint32_t i;

	for (i = 0; i < UL_MSG_WORDS; i++)
		msg.words[i] = i * 0x11111111u;

	for (i = 0; i < UL_MSG_WORDS; i++)
		TEST_ASSERT_EQUAL_UINT32(i * 0x11111111u, msg.words[i]);
}

void test_msg_copy_is_independent(void)
{
	ul_msg_t src = { .label = 0xDEAD, .words = {1, 2, 3, 4, 5, 6} };
	ul_msg_t dst = src;

	dst.label     = 0xBEEF;
	dst.words[0]  = 99;

	TEST_ASSERT_EQUAL_UINT32(0xDEADu, src.label);
	TEST_ASSERT_EQUAL_UINT32(1u, src.words[0]);
}

/* =========================================================================
 * ul_ep_call — single-threaded error paths only
 * (full round-trip tested in integration suite)
 * ========================================================================= */

/* CRASH PREVENTION */

void test_ep_call_invalid_ep_returns_error(void)
{
	ul_msg_t msg = { .label = 0 };

	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_call(UL_EP_INVALID, &msg));
}

void test_ep_call_null_msg_returns_error(void)
{
	ul_ep_t ep = ul_ep_create();

	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_call(ep, NULL));
}

void test_ep_call_own_ep_returns_edeadlk(void)
{
	/*
	 * A thread calling its own endpoint without a receiver would block
	 * forever — the kernel must detect and return -UL_EDEADLK.
	 */
	ul_ep_t  ep  = ul_ep_create();
	ul_msg_t msg = { .label = 0 };
	int      ret = ul_ep_call(ep, &msg);

	TEST_ASSERT_EQUAL_INT(-UL_EDEADLK, ret);
}

/* =========================================================================
 * ul_ep_recv — single-threaded error paths
 * ========================================================================= */

/* CRASH PREVENTION */

void test_ep_recv_invalid_ep_returns_error(void)
{
	ul_msg_t msg;
	ul_tid_t sender;

	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_recv(UL_EP_INVALID, &msg, &sender));
}

void test_ep_recv_null_msg_returns_error(void)
{
	ul_ep_t  ep = ul_ep_create();
	ul_tid_t sender;

	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_recv(ep, NULL, &sender));
}

void test_ep_recv_null_sender_returns_error(void)
{
	ul_ep_t  ep = ul_ep_create();
	ul_msg_t msg;

	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_recv(ep, &msg, NULL));
}

/* =========================================================================
 * ul_ep_reply — single-threaded error paths
 * ========================================================================= */

/* CRASH PREVENTION */

void test_ep_reply_invalid_sender_returns_error(void)
{
	ul_msg_t reply = { .label = 0 };

	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_reply(UL_TID_INVALID, &reply));
}

void test_ep_reply_non_blocked_sender_returns_error(void)
{
	/*
	 * Replying to a thread that is not blocked on ul_ep_call() must
	 * return an error, not corrupt the scheduler.
	 */
	ul_msg_t reply = { .label = 0 };

	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_reply(ul_thread_self(), &reply));
}

void test_ep_reply_null_reply_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_reply(ul_thread_self(), NULL));
}

/* =========================================================================
 * ul_ep_reply_recv — single-threaded error paths
 * ========================================================================= */

/* CRASH PREVENTION */

void test_ep_reply_recv_invalid_ep_returns_error(void)
{
	ul_msg_t reply = { .label = 0 };
	ul_msg_t next;
	ul_tid_t next_sender;

	int ret = ul_ep_reply_recv(UL_EP_INVALID, UL_TID_INVALID,
				   &reply, &next, &next_sender);
	TEST_ASSERT_LESS_THAN_INT(0, ret);
}

void test_ep_reply_recv_null_args_return_error(void)
{
	ul_ep_t ep = ul_ep_create();

	TEST_ASSERT_LESS_THAN_INT(
		0, ul_ep_reply_recv(ep, UL_TID_INVALID, NULL, NULL, NULL));
}

/* =========================================================================
 * ul_ep_grant
 * ========================================================================= */

/* HAPPY PATH */

void test_ep_grant_to_valid_target_returns_zero(void)
{
	ul_ep_t ep = ul_ep_create();

	/*
	 * Granting to self is valid — the caller already holds the capability,
	 * so this is a no-op that must succeed.
	 */
	TEST_ASSERT_EQUAL_INT(0, ul_ep_grant(ep, ul_thread_self()));
}

/* CRASH PREVENTION */

void test_ep_grant_invalid_ep_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_grant(UL_EP_INVALID, ul_thread_self()));
}

void test_ep_grant_invalid_target_returns_error(void)
{
	ul_ep_t ep = ul_ep_create();

	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_grant(ep, UL_TID_INVALID));
}

void test_ep_grant_both_invalid_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_ep_grant(UL_EP_INVALID, UL_TID_INVALID));
}

#endif /* 0 */
