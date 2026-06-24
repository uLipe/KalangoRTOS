/*
 * Userspace library (libul) test suite — covers ul_mutex, ul_sem, ul_queue,
 * ul_event, ul_timer (client), and ul_pipe.
 *
 * libul primitives are built on top of ul_notif_* and ul_ep_*.  They require
 * the kernel to be running (for notification object allocation), but most
 * single-object tests can be validated from a single thread.
 *
 * Blocking primitives (ul_mutex_lock with contention, ul_sem_wait when
 * empty, ul_queue_{send,recv} blocking paths) require two threads and belong
 * in the integration suite.
 *
 * All tests are inside #if 0 until the kernel is implemented.
 */

#include "unity.h"

/* #include <ul/microkernel.h> */
/* #include <ul/libul.h>             */

#if 0

/* =========================================================================
 * ul_mutex — built on a notification token bit
 * ========================================================================= */

/* HAPPY PATH */

void test_mutex_init_returns_zero(void)
{
	ul_mutex_t m;

	TEST_ASSERT_EQUAL_INT(0, ul_mutex_init(&m));
}

void test_mutex_init_token_bit_is_available(void)
{
	/*
	 * After init the mutex must be in the UNLOCKED state, i.e., the token
	 * bit is set in the notification.  A non-blocking poll should return it.
	 */
	ul_mutex_t m;

	ul_mutex_init(&m);
	/* Poll directly to inspect state without blocking. */
	uint32_t got = ul_notif_poll(m.notif, m.bit);

	TEST_ASSERT_NOT_EQUAL(0u, got);
}

void test_mutex_lock_then_unlock_cycle(void)
{
	ul_mutex_t m;

	ul_mutex_init(&m);
	ul_mutex_lock(&m);   /* acquires token */
	ul_mutex_unlock(&m); /* releases token */
	TEST_ASSERT_TRUE(1);
}

void test_mutex_unlock_restores_token_bit(void)
{
	ul_mutex_t m;

	ul_mutex_init(&m);
	ul_mutex_lock(&m);
	ul_mutex_unlock(&m);
	/* Token must be back; poll should return it. */
	uint32_t got = ul_notif_poll(m.notif, m.bit);

	TEST_ASSERT_NOT_EQUAL(0u, got);
}

/* CRASH PREVENTION */

void test_mutex_init_null_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_mutex_init(NULL));
}

/* =========================================================================
 * ul_sem — counting semaphore (up to 32 permits via notification bits)
 * ========================================================================= */

/* HAPPY PATH */

void test_sem_init_count_1_returns_zero(void)
{
	ul_sem_t s;

	TEST_ASSERT_EQUAL_INT(0, ul_sem_init(&s, 1));
}

void test_sem_init_count_8_returns_zero(void)
{
	ul_sem_t s;

	TEST_ASSERT_EQUAL_INT(0, ul_sem_init(&s, 8));
}

void test_sem_init_count_32_returns_zero(void)
{
	ul_sem_t s;

	TEST_ASSERT_EQUAL_INT(0, ul_sem_init(&s, 32));
}

void test_sem_post_then_poll_shows_extra_bit(void)
{
	ul_sem_t s;

	ul_sem_init(&s, 1);
	/* Wait (consume one permit). */
	ul_sem_wait(&s);
	/* Post one back. */
	ul_sem_post(&s);
	/* Poll should show one permit available. */
	uint32_t got = ul_notif_poll(s.notif, s.all_bits);

	TEST_ASSERT_NOT_EQUAL(0u, got);
}

/* EDGE CASES */

void test_sem_init_count_0_creates_fully_blocked_sem(void)
{
	ul_sem_t s;

	ul_sem_init(&s, 0);
	/* No bits signalled — poll should return 0. */
	TEST_ASSERT_EQUAL_HEX32(0u, ul_notif_poll(s.notif, s.all_bits));
}

/* CRASH PREVENTION */

void test_sem_init_count_33_returns_error(void)
{
	/* 33 permits would require 33 bits — exceeds 32-bit notification word. */
	ul_sem_t s;

	TEST_ASSERT_LESS_THAN_INT(0, ul_sem_init(&s, 33));
}

void test_sem_init_null_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_sem_init(NULL, 1));
}

/* =========================================================================
 * ul_queue — shared-memory FIFO with notification for flow control
 * ========================================================================= */

/* HAPPY PATH */

void test_queue_init_returns_zero(void)
{
	static uint8_t buf[256];
	ul_queue_t q;

	TEST_ASSERT_EQUAL_INT(0, ul_queue_init(&q, buf, sizeof(buf),
					       sizeof(uint32_t)));
}

void test_queue_capacity_equals_floor_of_buf_minus_header_over_item(void)
{
	static uint8_t buf[256];
	ul_queue_t q;

	ul_queue_init(&q, buf, sizeof(buf), sizeof(uint32_t));
	/* capacity = floor((256 - sizeof(ul_queue_buf_t)) / 4) */
	TEST_ASSERT_GREATER_THAN_UINT32(0u, q.buf->capacity);
}

void test_queue_send_nonblock_then_recv_nonblock_roundtrip(void)
{
	static uint8_t buf[256];
	ul_queue_t q;
	uint32_t   tx = 0xCAFEBABEu;
	uint32_t   rx = 0u;

	ul_queue_init(&q, buf, sizeof(buf), sizeof(uint32_t));
	TEST_ASSERT_EQUAL_INT(0, ul_queue_send(&q, &tx, false));
	TEST_ASSERT_EQUAL_INT(0, ul_queue_recv(&q, &rx, false));
	TEST_ASSERT_EQUAL_HEX32(tx, rx);
}

void test_queue_preserves_fifo_order(void)
{
	static uint8_t buf[256];
	ul_queue_t q;
	uint32_t   tx[4] = {1u, 2u, 3u, 4u};
	uint32_t   rx;

	ul_queue_init(&q, buf, sizeof(buf), sizeof(uint32_t));
	for (int i = 0; i < 4; i++)
		ul_queue_send(&q, &tx[i], false);
	for (int i = 0; i < 4; i++) {
		ul_queue_recv(&q, &rx, false);
		TEST_ASSERT_EQUAL_UINT32(tx[i], rx);
	}
}

/* EDGE CASES */

void test_queue_fill_then_drain(void)
{
	static uint8_t buf[128];
	ul_queue_t q;
	uint32_t   item = 0u;
	int        sent = 0;

	ul_queue_init(&q, buf, sizeof(buf), sizeof(uint32_t));
	while (ul_queue_send(&q, &item, false) == 0)
		sent++;

	TEST_ASSERT_GREATER_THAN_INT(0, sent);

	int recvd = 0;
	while (ul_queue_recv(&q, &item, false) == 0)
		recvd++;

	TEST_ASSERT_EQUAL_INT(sent, recvd);
}

/* CRASH PREVENTION */

void test_queue_send_full_nonblock_returns_error(void)
{
	static uint8_t buf[64];
	ul_queue_t q;
	uint32_t   item = 0u;

	ul_queue_init(&q, buf, sizeof(buf), sizeof(uint32_t));
	/* Fill queue. */
	while (ul_queue_send(&q, &item, false) == 0)
		;
	/* Next send must return an error, not block or crash. */
	TEST_ASSERT_LESS_THAN_INT(0, ul_queue_send(&q, &item, false));
}

void test_queue_recv_empty_nonblock_returns_error(void)
{
	static uint8_t buf[64];
	ul_queue_t q;
	uint32_t   item;

	ul_queue_init(&q, buf, sizeof(buf), sizeof(uint32_t));
	TEST_ASSERT_LESS_THAN_INT(0, ul_queue_recv(&q, &item, false));
}

void test_queue_init_null_buf_returns_error(void)
{
	ul_queue_t q;

	TEST_ASSERT_LESS_THAN_INT(0, ul_queue_init(&q, NULL, 64,
						   sizeof(uint32_t)));
}

void test_queue_init_item_larger_than_buf_returns_error(void)
{
	static uint8_t buf[4];
	ul_queue_t q;

	TEST_ASSERT_LESS_THAN_INT(0, ul_queue_init(&q, buf, sizeof(buf),
						   1024u));
}

/* =========================================================================
 * ul_event — event group (alias over ul_notif)
 * ========================================================================= */

/* HAPPY PATH */

void test_event_create_returns_valid_handle(void)
{
	ul_event_t ev = ul_event_create();

	TEST_ASSERT_NOT_EQUAL(UL_NOTIF_INVALID, ev);
}

void test_event_set_bits_visible_via_wait_any(void)
{
	ul_event_t ev = ul_event_create();

	ul_event_set(ev, 0x3u);
	uint32_t got = ul_event_wait_any(ev, 0x3u);

	TEST_ASSERT_EQUAL_HEX32(0x3u, got);
}

void test_event_wait_all_requires_all_bits_to_be_set(void)
{
	ul_event_t ev = ul_event_create();

	ul_event_set(ev, 0x7u);
	uint32_t got = ul_event_wait_all(ev, 0x7u);

	TEST_ASSERT_EQUAL_HEX32(0x7u, got);
}

void test_event_clear_removes_specific_bits(void)
{
	ul_event_t ev = ul_event_create();

	ul_event_set(ev, 0xFu);
	ul_event_clear(ev, 0x3u);
	uint32_t got = ul_notif_poll(ev, 0xFFFFFFFFu);

	TEST_ASSERT_EQUAL_HEX32(0xCu, got);
}

/* EDGE CASES */

void test_event_set_zero_is_noop(void)
{
	ul_event_t ev = ul_event_create();

	ul_event_set(ev, 0u);
	TEST_ASSERT_EQUAL_HEX32(0u, ul_notif_poll(ev, 0xFFFFFFFFu));
}

void test_event_clear_more_bits_than_set_does_not_crash(void)
{
	ul_event_t ev = ul_event_create();

	ul_event_set(ev, 0x1u);
	ul_event_clear(ev, 0xFFFFFFFFu);  /* clear more than was set */
	TEST_ASSERT_EQUAL_HEX32(0u, ul_notif_poll(ev, 0xFFFFFFFFu));
}

/* =========================================================================
 * ul_timer (client) — validates IPC argument passing; no actual timer
 * ========================================================================= */

/* CRASH PREVENTION */

void test_timer_oneshot_invalid_ep_returns_error(void)
{
	ul_notif_t n   = ul_notif_create();
	ul_timer_id_t r = ul_timer_oneshot(UL_EP_INVALID, 100u, n, 1u);

	TEST_ASSERT_LESS_THAN_INT(0, (int)r);
}

void test_timer_periodic_invalid_ep_returns_error(void)
{
	ul_notif_t n   = ul_notif_create();
	ul_timer_id_t r = ul_timer_periodic(UL_EP_INVALID, 100u, n, 1u);

	TEST_ASSERT_LESS_THAN_INT(0, (int)r);
}

void test_timer_cancel_invalid_id_returns_error(void)
{
	ul_ep_t ep = ul_ep_create();
	int     r  = ul_timer_cancel(ep, (ul_timer_id_t)0);

	TEST_ASSERT_LESS_THAN_INT(0, r);
}

void test_timer_oneshot_zero_ticks_returns_error(void)
{
	ul_ep_t    svc = ul_ep_create();
	ul_notif_t n   = ul_notif_create();

	TEST_ASSERT_LESS_THAN_INT(0,
		(int)ul_timer_oneshot(svc, 0u, n, 1u));
}

void test_timer_oneshot_zero_bit_mask_returns_error(void)
{
	ul_ep_t    svc = ul_ep_create();
	ul_notif_t n   = ul_notif_create();

	TEST_ASSERT_LESS_THAN_INT(0,
		(int)ul_timer_oneshot(svc, 100u, n, 0u));
}

/* =========================================================================
 * ul_pipe — byte-stream wrapper over ul_queue
 * ========================================================================= */

/* HAPPY PATH */

void test_pipe_init_returns_zero(void)
{
	static uint8_t buf[64];
	ul_pipe_t p;

	TEST_ASSERT_EQUAL_INT(0, ul_pipe_init(&p, buf, sizeof(buf)));
}

void test_pipe_write_then_read_single_byte(void)
{
	static uint8_t buf[64];
	ul_pipe_t p;
	uint8_t   tx = 0xABu;
	uint8_t   rx = 0u;

	ul_pipe_init(&p, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_INT(1, ul_pipe_write(&p, &tx, 1u, false));
	TEST_ASSERT_EQUAL_INT(1, ul_pipe_read(&p, &rx, 1u, false));
	TEST_ASSERT_EQUAL_HEX8(tx, rx);
}

void test_pipe_write_multiple_bytes_preserves_order(void)
{
	static uint8_t buf[64];
	ul_pipe_t p;
	uint8_t   tx[4] = {0x11u, 0x22u, 0x33u, 0x44u};
	uint8_t   rx[4] = {0};

	ul_pipe_init(&p, buf, sizeof(buf));
	TEST_ASSERT_EQUAL_INT(4, ul_pipe_write(&p, tx, 4u, false));
	TEST_ASSERT_EQUAL_INT(4, ul_pipe_read(&p, rx, 4u, false));
	TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, rx, 4u);
}

/* CRASH PREVENTION */

void test_pipe_read_empty_nonblock_returns_error(void)
{
	static uint8_t buf[64];
	ul_pipe_t p;
	uint8_t   rx;

	ul_pipe_init(&p, buf, sizeof(buf));
	TEST_ASSERT_LESS_THAN_INT(0, ul_pipe_read(&p, &rx, 1u, false));
}

void test_pipe_write_full_nonblock_returns_error(void)
{
	static uint8_t buf[8];
	ul_pipe_t p;
	uint8_t   tx[16] = {0};

	ul_pipe_init(&p, buf, sizeof(buf));
	/* Write until full. */
	while (ul_pipe_write(&p, tx, 1u, false) == 1)
		;
	/* Next byte must fail gracefully. */
	TEST_ASSERT_LESS_THAN_INT(0, ul_pipe_write(&p, tx, 1u, false));
}

#endif /* 0 */
