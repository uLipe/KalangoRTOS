/*
 * IRQ API test suite — covers ul_irq_bind, enable, disable, ack.
 *
 * Hardware IRQ delivery (the IRQ actually firing and signalling a
 * notification) requires a running interrupt source and belongs in the
 * hardware integration suite.  Tests here verify single-threaded observable
 * behaviour: argument validation, resource accounting, and binding state.
 *
 * All tests are inside #if 0 until the kernel is implemented.
 */

#include "unity.h"

/* #include <ul/microkernel.h> */

#if 0

/*
 * SRPN 0 is reserved by the TriCore hardware interrupt architecture.
 * Use 1–255 for valid priorities in tests.
 */
#define TEST_SRPN_VALID    10u
#define TEST_SRPN_RESERVED  0u

/* =========================================================================
 * ul_irq_bind
 * ========================================================================= */

/* HAPPY PATH */

void test_irq_bind_valid_srpn_and_notif_returns_zero(void)
{
	ul_notif_t n   = ul_notif_create();
	int        ret = ul_irq_bind(TEST_SRPN_VALID, n, 1u << 0);

	TEST_ASSERT_NOT_EQUAL(UL_NOTIF_INVALID, n);
	TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_irq_bind_bit_0(void)
{
	ul_notif_t n = ul_notif_create();

	TEST_ASSERT_EQUAL_INT(0, ul_irq_bind(TEST_SRPN_VALID, n, 1u << 0));
}

void test_irq_bind_bit_31(void)
{
	ul_notif_t n = ul_notif_create();

	TEST_ASSERT_EQUAL_INT(0, ul_irq_bind(TEST_SRPN_VALID + 1u, n, 1u << 31));
}

void test_irq_bind_rebind_same_srpn_to_different_notif_returns_zero(void)
{
	ul_notif_t a = ul_notif_create();
	ul_notif_t b = ul_notif_create();

	TEST_ASSERT_EQUAL_INT(0, ul_irq_bind(TEST_SRPN_VALID, a, 1u));
	/* Rebind to a different notification — must succeed (replaces binding). */
	TEST_ASSERT_EQUAL_INT(0, ul_irq_bind(TEST_SRPN_VALID, b, 2u));
}

/* EDGE CASES */

void test_irq_bind_max_valid_srpn(void)
{
	ul_notif_t n = ul_notif_create();

	TEST_ASSERT_EQUAL_INT(0, ul_irq_bind(255u, n, 1u));
}

void test_irq_bind_multiple_srpns_distinct_notifs(void)
{
	ul_notif_t a = ul_notif_create();
	ul_notif_t b = ul_notif_create();

	TEST_ASSERT_EQUAL_INT(0, ul_irq_bind(TEST_SRPN_VALID,      a, 1u));
	TEST_ASSERT_EQUAL_INT(0, ul_irq_bind(TEST_SRPN_VALID + 1u, b, 1u));
}

/* CRASH PREVENTION */

void test_irq_bind_invalid_notif_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_irq_bind(TEST_SRPN_VALID,
						 UL_NOTIF_INVALID, 1u));
}

void test_irq_bind_reserved_srpn_zero_returns_error(void)
{
	ul_notif_t n = ul_notif_create();

	TEST_ASSERT_LESS_THAN_INT(0, ul_irq_bind(TEST_SRPN_RESERVED, n, 1u));
}

void test_irq_bind_bit_zero_mask_returns_error(void)
{
	/*
	 * A zero bitmask would mean "signal nothing" — meaningless binding.
	 * The kernel should reject it.
	 */
	ul_notif_t n = ul_notif_create();

	TEST_ASSERT_LESS_THAN_INT(0, ul_irq_bind(TEST_SRPN_VALID, n, 0u));
}

/* =========================================================================
 * ul_irq_enable / ul_irq_disable
 * ========================================================================= */

/* HAPPY PATH */

void test_irq_enable_after_bind_returns_zero(void)
{
	ul_notif_t n = ul_notif_create();

	ul_irq_bind(TEST_SRPN_VALID, n, 1u);
	TEST_ASSERT_EQUAL_INT(0, ul_irq_enable(TEST_SRPN_VALID));
}

void test_irq_disable_after_enable_returns_zero(void)
{
	ul_notif_t n = ul_notif_create();

	ul_irq_bind(TEST_SRPN_VALID, n, 1u);
	ul_irq_enable(TEST_SRPN_VALID);
	TEST_ASSERT_EQUAL_INT(0, ul_irq_disable(TEST_SRPN_VALID));
}

void test_irq_enable_disable_repeated_does_not_crash(void)
{
	ul_notif_t n = ul_notif_create();

	ul_irq_bind(TEST_SRPN_VALID, n, 1u);
	for (int i = 0; i < 8; i++) {
		ul_irq_enable(TEST_SRPN_VALID);
		ul_irq_disable(TEST_SRPN_VALID);
	}
	TEST_ASSERT_TRUE(1);
}

/* EDGE CASES */

void test_irq_disable_already_disabled_is_noop(void)
{
	ul_notif_t n = ul_notif_create();

	ul_irq_bind(TEST_SRPN_VALID, n, 1u);
	/* Never enabled — disable on a bound-but-disabled IRQ must succeed. */
	TEST_ASSERT_EQUAL_INT(0, ul_irq_disable(TEST_SRPN_VALID));
}

/* CRASH PREVENTION */

void test_irq_enable_unbound_srpn_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_irq_enable(TEST_SRPN_VALID + 50u));
}

void test_irq_disable_unbound_srpn_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_irq_disable(TEST_SRPN_VALID + 50u));
}

void test_irq_enable_reserved_srpn_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_irq_enable(TEST_SRPN_RESERVED));
}

/* =========================================================================
 * ul_irq_ack
 * ========================================================================= */

/* HAPPY PATH */

void test_irq_ack_enabled_srpn_returns_zero(void)
{
	ul_notif_t n = ul_notif_create();

	ul_irq_bind(TEST_SRPN_VALID, n, 1u);
	ul_irq_enable(TEST_SRPN_VALID);
	/*
	 * Acknowledging without a pending IRQ must be a no-op that succeeds.
	 * The real test (ack after actual IRQ) requires hardware integration.
	 */
	TEST_ASSERT_EQUAL_INT(0, ul_irq_ack(TEST_SRPN_VALID));
}

/* CRASH PREVENTION */

void test_irq_ack_unbound_srpn_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_irq_ack(TEST_SRPN_VALID + 50u));
}

void test_irq_ack_reserved_srpn_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_irq_ack(TEST_SRPN_RESERVED));
}

#endif /* 0 */
