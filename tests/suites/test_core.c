#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

void test_core_run(void)
{
    RUN_TEST(test_core_ticks_increment);
    RUN_TEST(test_core_heap_reports_free_bytes);
    RUN_TEST(test_core_current_task_not_null);
}

void test_core_ticks_increment(void)
{
    uint32_t t0 = Kalango_GetCurrentTicks();

    test_sleep_ticks(5);

    TEST_ASSERT_GREATER_THAN_UINT32(t0, Kalango_GetCurrentTicks());
}

void test_core_heap_reports_free_bytes(void)
{
    uint32_t free_bytes = Kalango_GetHeapFreeBytes();

    TEST_ASSERT_GREATER_THAN(0, (int)free_bytes);
}

void test_core_current_task_not_null(void)
{
    TEST_ASSERT_NOT_NULL(Kalango_GetCurrentTaskId());
}
