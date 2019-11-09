#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

static SemaphoreId worker_done;
static volatile uint32_t worker_ran;

static void worker_task(void *arg)
{
    (void)arg;
    worker_ran = 1;
    Kalango_SemaphoreGive(worker_done, 1);
}

void test_tasks_run(void)
{
    RUN_TEST(test_task_create_returns_valid_id);
    RUN_TEST(test_task_yield_runs_other_task);
    RUN_TEST(test_task_suspend_and_resume);
    RUN_TEST(test_task_set_get_priority);
    RUN_TEST(test_task_delete_defers_cleanup);
}

void test_task_create_returns_valid_id(void)
{
    TaskSettings settings = {
        .priority = 2,
        .stack_size = 512,
        .function = worker_task,
        .arg = NULL,
    };

    TaskId id = Kalango_TaskCreate(&settings);
    TEST_ASSERT_NOT_NULL(id);
}

void test_task_yield_runs_other_task(void)
{
    worker_done = Kalango_SemaphoreCreate(0, 1);
    worker_ran = 0;

    TaskSettings settings = {
        .priority = CONFIG_PRIORITY_LEVELS - 2,
        .stack_size = 512,
        .function = worker_task,
        .arg = NULL,
    };

    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&settings));

    Kalango_TaskYield();
    test_sleep_ticks(2);

    TEST_ASSERT_EQUAL_UINT32(1, worker_ran);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(worker_done, 10));
}

void test_task_suspend_and_resume(void)
{
    TaskId worker;
    TaskSettings settings = {
        .priority = 3,
        .stack_size = 512,
        .function = worker_task,
        .arg = NULL,
    };

    worker_done = Kalango_SemaphoreCreate(0, 1);
    worker_ran = 0;
    worker = Kalango_TaskCreate(&settings);
    TEST_ASSERT_NOT_NULL(worker);

    TEST_ASSERT_EQUAL(kSuccess, Kalango_TaskSuspend(worker));
    test_sleep_ticks(3);
    TEST_ASSERT_EQUAL_UINT32(0, worker_ran);

    TEST_ASSERT_EQUAL(kSuccess, Kalango_TaskResume(worker));
    test_sleep_ticks(3);
    TEST_ASSERT_EQUAL_UINT32(1, worker_ran);
}

void test_task_set_get_priority(void)
{
    TaskId worker;
    TaskSettings settings = {
        .priority = 2,
        .stack_size = 512,
        .function = worker_task,
        .arg = NULL,
    };

    worker = Kalango_TaskCreate(&settings);
    TEST_ASSERT_NOT_NULL(worker);

    TEST_ASSERT_EQUAL_UINT32(2, Kalango_TaskGetPriority(worker));
    TEST_ASSERT_EQUAL_UINT32(2, Kalango_TaskSetPriority(worker, 5));
    TEST_ASSERT_EQUAL_UINT32(5, Kalango_TaskGetPriority(worker));
}

void test_task_delete_defers_cleanup(void)
{
    TaskId worker;
    TaskSettings settings = {
        .priority = 1,
        .stack_size = 512,
        .function = worker_task,
        .arg = NULL,
    };

    worker = Kalango_TaskCreate(&settings);
    TEST_ASSERT_NOT_NULL(worker);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_TaskDelete(worker));
    test_sleep_ticks(5);
    TEST_PASS();
}
