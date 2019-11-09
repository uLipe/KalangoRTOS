#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

static volatile uint32_t timer_fired;

static void timer_cb(void *user)
{
    (void)user;
    timer_fired++;
}

void test_timer_run(void)
{
    RUN_TEST(test_timer_one_shot);
    RUN_TEST(test_timer_periodic);
    RUN_TEST(test_timer_stop);
}

void test_timer_one_shot(void)
{
    TimerId t = Kalango_TimerCreate(timer_cb, 3, 0, NULL);

    timer_fired = 0;
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_TimerStart(t));
    test_sleep_ticks(6);
    TEST_ASSERT_GREATER_OR_EQUAL(1, (int)timer_fired);
}

void test_timer_periodic(void)
{
    TimerId t = Kalango_TimerCreate(timer_cb, 2, 2, NULL);

    timer_fired = 0;
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_TimerStart(t));
    test_sleep_ticks(8);
    TEST_ASSERT_GREATER_OR_EQUAL(2, (int)timer_fired);
    Kalango_TimerStop(t);
}

void test_timer_stop(void)
{
    TimerId t = Kalango_TimerCreate(timer_cb, 2, 2, NULL);
    uint32_t before;

    timer_fired = 0;
    TEST_ASSERT_EQUAL(kSuccess, Kalango_TimerStart(t));
    test_sleep_ticks(3);
    before = timer_fired;
    TEST_ASSERT_EQUAL(kSuccess, Kalango_TimerStop(t));
    test_sleep_ticks(6);
    TEST_ASSERT_EQUAL_UINT32(before, timer_fired);
}
