#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

#if CONFIG_ENABLE_SOFTIRQ > 0

/* -------------------------------------------------------------------
 * Shared state used by handler callbacks.
 * Each test resets these before running.
 * ------------------------------------------------------------------- */
static SemaphoreId             softirq_done;
static volatile uint32_t       handler_call_count;
static volatile const void    *handler_last_data;

static void counting_handler(void *data)
{
    handler_last_data = data;
    handler_call_count++;
    Kalango_SemaphoreGive(softirq_done, 1);
}

/* -------------------------------------------------------------------
 * test_softirq_trigger_calls_handler
 *
 * Register a handler on vector 0, trigger it, and confirm it executes
 * exactly once (proved via semaphore synchronisation with kSoftIrqD).
 * ------------------------------------------------------------------- */
void test_softirq_trigger_calls_handler(void)
{
    softirq_done       = Kalango_SemaphoreCreate(0, 4);
    handler_call_count = 0;

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SoftIrqRequest(0, counting_handler));
    Kalango_SoftIrqTrigger(0, NULL);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(softirq_done, 20));
    TEST_ASSERT_EQUAL_UINT32(1, handler_call_count);

    Kalango_SoftIrqRequest(0, NULL);
}

/* -------------------------------------------------------------------
 * test_softirq_handler_receives_data
 *
 * The data pointer passed to SoftIrqTrigger must arrive unchanged in
 * the registered handler.
 * ------------------------------------------------------------------- */
void test_softirq_handler_receives_data(void)
{
    static uint32_t sentinel = 0xDEADBEEFU;

    softirq_done      = Kalango_SemaphoreCreate(0, 1);
    handler_last_data = NULL;
    handler_call_count = 0;

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SoftIrqRequest(1, counting_handler));
    Kalango_SoftIrqTrigger(1, &sentinel);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(softirq_done, 20));
    TEST_ASSERT_EQUAL_PTR(&sentinel, (const void *)handler_last_data);

    Kalango_SoftIrqRequest(1, NULL);
}

/* -------------------------------------------------------------------
 * test_softirq_invalid_vector_rejected
 *
 * Requesting a vector index >= CONFIG_SOFTIRQ_MAX_VECTORS must return
 * kErrorInvalidParam without touching any internal state.
 * ------------------------------------------------------------------- */
void test_softirq_invalid_vector_rejected(void)
{
    TEST_ASSERT_EQUAL(kErrorInvalidParam,
                      Kalango_SoftIrqRequest(CONFIG_SOFTIRQ_MAX_VECTORS, counting_handler));
}

/* -------------------------------------------------------------------
 * test_softirq_multiple_vectors_dispatched
 *
 * Triggering vector 0 must dispatch only vector 0's handler; vector 1
 * must remain silent until triggered independently.
 * ------------------------------------------------------------------- */
static volatile uint32_t  vec1_ran;
static SemaphoreId         vec1_done;

static void vec1_handler(void *data)
{
    (void)data;
    vec1_ran = 1;
    Kalango_SemaphoreGive(vec1_done, 1);
}

void test_softirq_multiple_vectors_dispatched(void)
{
    softirq_done       = Kalango_SemaphoreCreate(0, 4);
    vec1_done          = Kalango_SemaphoreCreate(0, 4);
    handler_call_count = 0;
    vec1_ran           = 0;

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SoftIrqRequest(0, counting_handler));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SoftIrqRequest(1, vec1_handler));

    Kalango_SoftIrqTrigger(0, NULL);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(softirq_done, 20));
    TEST_ASSERT_EQUAL_UINT32(1, handler_call_count);
    TEST_ASSERT_EQUAL_UINT32(0, vec1_ran);

    Kalango_SoftIrqTrigger(1, NULL);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(vec1_done, 20));
    TEST_ASSERT_EQUAL_UINT32(1, vec1_ran);

    Kalango_SoftIrqRequest(0, NULL);
    Kalango_SoftIrqRequest(1, NULL);
}

/* -------------------------------------------------------------------
 * test_softirq_no_handler_no_crash
 *
 * Triggering a vector with no registered handler must not crash or
 * stall the system: kSoftIrqD wakes, finds a NULL entry, skips it.
 * ------------------------------------------------------------------- */
void test_softirq_no_handler_no_crash(void)
{
    Kalango_SoftIrqRequest(2, NULL);
    Kalango_SoftIrqTrigger(2, NULL);
    test_sleep_ticks(5);
    TEST_PASS();
}

/* -------------------------------------------------------------------
 * test_softirq_same_vector_coalesces
 *
 * Multiple triggers of the same vector OR into one bitmap bit, so
 * kSoftIrqD runs the handler exactly once per drain cycle (Linux-style
 * coalescing semantics).  A second semaphore take must time out,
 * confirming no spurious second invocation occurred.
 * ------------------------------------------------------------------- */
void test_softirq_same_vector_coalesces(void)
{
    softirq_done       = Kalango_SemaphoreCreate(0, 4);
    handler_call_count = 0;

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SoftIrqRequest(3, counting_handler));

    Kalango_SoftIrqTrigger(3, NULL);
    Kalango_SoftIrqTrigger(3, NULL);

    TEST_ASSERT_EQUAL(kSuccess,       Kalango_SemaphoreTake(softirq_done, 20));
    TEST_ASSERT_EQUAL_UINT32(1,       handler_call_count);
    TEST_ASSERT_EQUAL(kErrorTimeout,  Kalango_SemaphoreTake(softirq_done, 10));

    Kalango_SoftIrqRequest(3, NULL);
}

void test_softirq_run(void)
{
    RUN_TEST(test_softirq_trigger_calls_handler);
    RUN_TEST(test_softirq_handler_receives_data);
    RUN_TEST(test_softirq_invalid_vector_rejected);
    RUN_TEST(test_softirq_multiple_vectors_dispatched);
    RUN_TEST(test_softirq_no_handler_no_crash);
    RUN_TEST(test_softirq_same_vector_coalesces);
}

#else  /* CONFIG_ENABLE_SOFTIRQ == 0 */

void test_softirq_run(void) {}

#endif /* CONFIG_ENABLE_SOFTIRQ */
