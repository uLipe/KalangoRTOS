/*
 * Notification API test suite — covers ul_notif_create, signal, wait, poll,
 * and the combined ul_ep_recv_or_notif.
 *
 * NOTE: ul_notif_wait() blocks the calling thread and requires a second
 * thread (or ISR) to unblock it.  Those tests belong in the integration
 * suite.  Tests here verify single-threaded observable behaviour via poll.
 *
 * All tests are inside #if 0 until the kernel is implemented.
 */

#include "unity.h"

/* #include <sys/ulipe_microkernel.h> */

#if 0

/* =========================================================================
 * ul_notif_create
 * ========================================================================= */

/* HAPPY PATH */

void test_notif_create_returns_valid_handle(void)
{
	ul_notif_t n = ul_notif_create();

	TEST_ASSERT_NOT_EQUAL(UL_NOTIF_INVALID, n);
	TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, n);
}

void test_notif_create_multiple_returns_distinct_handles(void)
{
	ul_notif_t a = ul_notif_create();
	ul_notif_t b = ul_notif_create();

	TEST_ASSERT_NOT_EQUAL(UL_NOTIF_INVALID, a);
	TEST_ASSERT_NOT_EQUAL(UL_NOTIF_INVALID, b);
	TEST_ASSERT_NOT_EQUAL(a, b);
}

/* EDGE CASES */

void test_notif_create_fresh_object_has_no_bits(void)
{
	ul_notif_t n   = ul_notif_create();
	uint32_t   got = ul_notif_poll(n, 0xFFFFFFFFu);

	TEST_ASSERT_EQUAL_HEX32(0u, got);
}

void test_notif_create_up_to_system_limit_returns_error(void)
{
	int failed = 0;
	for (int i = 0; i < 256; i++) {
		ul_notif_t n = ul_notif_create();
		if (n == UL_NOTIF_INVALID) {
			failed++;
			break;
		}
	}
	TEST_ASSERT_GREATER_THAN_INT(0, failed);
}

/* =========================================================================
 * ul_notif_signal
 * ========================================================================= */

/* HAPPY PATH */

void test_notif_signal_single_bit_visible_via_poll(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 1u << 3);
	TEST_ASSERT_EQUAL_HEX32(1u << 3, ul_notif_poll(n, 1u << 3));
}

void test_notif_signal_multiple_bits_visible_via_poll(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0x5u);
	ul_notif_signal(n, 0xAu);
	TEST_ASSERT_EQUAL_HEX32(0xFu, ul_notif_poll(n, 0xFFFFFFFFu));
}

void test_notif_signal_all_bits(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0xFFFFFFFFu);
	TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFFu, ul_notif_poll(n, 0xFFFFFFFFu));
}

/* EDGE CASES */

void test_notif_signal_bit_0(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 1u << 0);
	TEST_ASSERT_EQUAL_HEX32(1u, ul_notif_poll(n, 0xFFFFFFFFu));
}

void test_notif_signal_bit_31(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 1u << 31);
	TEST_ASSERT_EQUAL_HEX32(1u << 31, ul_notif_poll(n, 0xFFFFFFFFu));
}

void test_notif_signal_zero_is_noop(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0u);
	TEST_ASSERT_EQUAL_HEX32(0u, ul_notif_poll(n, 0xFFFFFFFFu));
}

void test_notif_signal_accumulates_across_calls(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0x1u);
	ul_notif_signal(n, 0x2u);
	ul_notif_signal(n, 0x4u);
	TEST_ASSERT_EQUAL_HEX32(0x7u, ul_notif_poll(n, 0xFFFFFFFFu));
}

void test_notif_signal_idempotent(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0xFu);
	ul_notif_signal(n, 0xFu);  /* OR is idempotent */
	TEST_ASSERT_EQUAL_HEX32(0xFu, ul_notif_poll(n, 0xFFFFFFFFu));
}

/* CRASH PREVENTION */

void test_notif_signal_invalid_handle_does_not_crash(void)
{
	/*
	 * Signalling an invalid handle must be silently ignored — never fault.
	 * Common in ISR paths where the binding might not yet be set up.
	 */
	ul_notif_signal(UL_NOTIF_INVALID, 0xFFu);
	TEST_ASSERT_TRUE(1);
}

/* =========================================================================
 * ul_notif_poll
 * ========================================================================= */

/* HAPPY PATH */

void test_notif_poll_returns_matched_bits_only(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0xFFu);
	/* Poll with mask 0x0F — should get only the lower nibble. */
	TEST_ASSERT_EQUAL_HEX32(0x0Fu, ul_notif_poll(n, 0x0Fu));
}

void test_notif_poll_clears_matched_bits(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0xFFu);
	ul_notif_poll(n, 0x0Fu);  /* clears lower nibble */

	/* Only upper nibble should remain. */
	TEST_ASSERT_EQUAL_HEX32(0xF0u, ul_notif_poll(n, 0xFFFFFFFFu));
}

void test_notif_poll_does_not_clear_unmatched_bits(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0x3u);
	ul_notif_poll(n, 0x1u);    /* clears bit 0 */

	TEST_ASSERT_EQUAL_HEX32(0x2u, ul_notif_poll(n, 0xFFFFFFFFu));
}

/* EDGE CASES */

void test_notif_poll_after_poll_returns_zero(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0xFu);
	ul_notif_poll(n, 0xFFFFFFFFu);  /* consume all */
	TEST_ASSERT_EQUAL_HEX32(0u, ul_notif_poll(n, 0xFFFFFFFFu));
}

void test_notif_poll_zero_mask_returns_zero(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0xFFu);
	TEST_ASSERT_EQUAL_HEX32(0u, ul_notif_poll(n, 0u));
}

void test_notif_poll_unmatched_mask_returns_zero(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0xF0u);
	/* Poll for lower nibble bits that were never set. */
	TEST_ASSERT_EQUAL_HEX32(0u, ul_notif_poll(n, 0x0Fu));
}

void test_notif_poll_unmatched_mask_does_not_clear_bits(void)
{
	ul_notif_t n = ul_notif_create();

	ul_notif_signal(n, 0xF0u);
	ul_notif_poll(n, 0x0Fu);         /* miss */
	TEST_ASSERT_EQUAL_HEX32(0xF0u, ul_notif_poll(n, 0xFFFFFFFFu));
}

/* CRASH PREVENTION */

void test_notif_poll_invalid_handle_returns_zero(void)
{
	/*
	 * Polling an invalid handle must return 0, not fault.
	 * Callers may check validity lazily.
	 */
	uint32_t got = ul_notif_poll(UL_NOTIF_INVALID, 0xFFFFFFFFu);

	TEST_ASSERT_EQUAL_HEX32(0u, got);
}

/* =========================================================================
 * ul_ep_recv_or_notif — error paths only (blocking path = integration test)
 * ========================================================================= */

/* CRASH PREVENTION */

void test_ep_recv_or_notif_invalid_ep_returns_error(void)
{
	ul_notif_t n = ul_notif_create();
	ul_msg_t   msg;
	ul_tid_t   sender;
	uint32_t   bits;

	int ret = ul_ep_recv_or_notif(UL_EP_INVALID, n, 0x1u,
				      &msg, &sender, &bits);
	TEST_ASSERT_LESS_THAN_INT(0, ret);
}

void test_ep_recv_or_notif_invalid_notif_returns_error(void)
{
	ul_ep_t  ep = ul_ep_create();
	ul_msg_t msg;
	ul_tid_t sender;
	uint32_t bits;

	int ret = ul_ep_recv_or_notif(ep, UL_NOTIF_INVALID, 0x1u,
				      &msg, &sender, &bits);
	TEST_ASSERT_LESS_THAN_INT(0, ret);
}

void test_ep_recv_or_notif_null_out_args_returns_error(void)
{
	ul_ep_t    ep    = ul_ep_create();
	ul_notif_t notif = ul_notif_create();

	TEST_ASSERT_LESS_THAN_INT(
		0, ul_ep_recv_or_notif(ep, notif, 0x1u, NULL, NULL, NULL));
}

#endif /* 0 */
