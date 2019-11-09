#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

typedef struct {
    uint32_t value;
} queue_msg_t;

void test_queue_run(void)
{
    RUN_TEST(test_queue_insert_remove);
    RUN_TEST(test_queue_peek);
    RUN_TEST(test_queue_full_no_wait);
    RUN_TEST(test_queue_empty_no_wait);
}

void test_queue_insert_remove(void)
{
    QueueId q = Kalango_QueueCreate(4, sizeof(queue_msg_t));
    queue_msg_t out = { .value = 0xA5A5A5A5 };
    queue_msg_t in = { .value = 0 };
    uint32_t sz = sizeof(in);

    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_QueueInsert(q, &out, sizeof(out), KERNEL_NO_WAIT));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_QueueRemove(q, &in, &sz, KERNEL_NO_WAIT));
    TEST_ASSERT_EQUAL_UINT32(0xA5A5A5A5, in.value);
}

void test_queue_peek(void)
{
    QueueId q = Kalango_QueueCreate(2, sizeof(queue_msg_t));
    queue_msg_t out = { .value = 42 };
    queue_msg_t in = { .value = 0 };
    uint32_t sz = sizeof(in);

    TEST_ASSERT_EQUAL(kSuccess, Kalango_QueueInsert(q, &out, sizeof(out), KERNEL_NO_WAIT));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_QueuePeek(q, &in, &sz, KERNEL_NO_WAIT));
    TEST_ASSERT_EQUAL_UINT32(42, in.value);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_QueueRemove(q, &in, &sz, KERNEL_NO_WAIT));
}

void test_queue_full_no_wait(void)
{
    QueueId q = Kalango_QueueCreate(1, sizeof(queue_msg_t));
    queue_msg_t msg = { .value = 1 };

    TEST_ASSERT_EQUAL(kSuccess, Kalango_QueueInsert(q, &msg, sizeof(msg), KERNEL_NO_WAIT));
    TEST_ASSERT_EQUAL(kErrorBufferFull, Kalango_QueueInsert(q, &msg, sizeof(msg), KERNEL_NO_WAIT));
}

void test_queue_empty_no_wait(void)
{
    QueueId q = Kalango_QueueCreate(2, sizeof(queue_msg_t));
    queue_msg_t in = { .value = 0 };
    uint32_t sz = sizeof(in);

    TEST_ASSERT_EQUAL(kErrorBufferEmpty, Kalango_QueueRemove(q, &in, &sz, KERNEL_NO_WAIT));
}
