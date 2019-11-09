#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

void test_semaphore_run(void)
{
    RUN_TEST(test_semaphore_create_take_give);
    RUN_TEST(test_semaphore_no_wait_unavailable);
    RUN_TEST(test_semaphore_timeout);
    RUN_TEST(test_semaphore_limit_cap);
}

void test_semaphore_create_take_give(void)
{
    SemaphoreId sem = Kalango_SemaphoreCreate(0, 4);

    TEST_ASSERT_NOT_NULL(sem);
    TEST_ASSERT_EQUAL(kStatusSemaphoreUnavailable, Kalango_SemaphoreTake(sem, KERNEL_NO_WAIT));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreGive(sem, 1));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(sem, KERNEL_NO_WAIT));
}

void test_semaphore_no_wait_unavailable(void)
{
    SemaphoreId sem = Kalango_SemaphoreCreate(0, 1);

    TEST_ASSERT_EQUAL(kStatusSemaphoreUnavailable, Kalango_SemaphoreTake(sem, KERNEL_NO_WAIT));
}

void test_semaphore_timeout(void)
{
    SemaphoreId sem = Kalango_SemaphoreCreate(0, 1);

    TEST_ASSERT_EQUAL(kErrorTimeout, Kalango_SemaphoreTake(sem, 2));
}

void test_semaphore_limit_cap(void)
{
    SemaphoreId sem = Kalango_SemaphoreCreate(0, 2);

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreGive(sem, 5));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(sem, KERNEL_NO_WAIT));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(sem, KERNEL_NO_WAIT));
    TEST_ASSERT_EQUAL(kStatusSemaphoreUnavailable, Kalango_SemaphoreTake(sem, KERNEL_NO_WAIT));
}
