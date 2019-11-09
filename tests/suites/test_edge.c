#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

void test_edge_run(void)
{
    RUN_TEST(test_edge_null_task_suspend);
    RUN_TEST(test_edge_sleep_zero);
    RUN_TEST(test_edge_semaphore_null);
}

void test_edge_null_task_suspend(void)
{
    TEST_ASSERT_EQUAL(kErrorInvalidParam, Kalango_TaskSuspend(NULL));
}

void test_edge_sleep_zero(void)
{
    TEST_ASSERT_EQUAL(kErrorInvalidParam, Kalango_Sleep(0));
}

void test_edge_semaphore_null(void)
{
    TEST_ASSERT_EQUAL(kErrorInvalidParam, Kalango_SemaphoreGive(NULL, 1));
}
