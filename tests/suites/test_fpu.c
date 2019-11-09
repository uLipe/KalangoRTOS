#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

static SemaphoreId fpu_done;
static volatile float fpu_result;

static void fpu_worker(void *arg)
{
    volatile float a = 1.5f;
    volatile float b = 2.0f;
    (void)arg;

    fpu_result = a * b;
    Kalango_SemaphoreGive(fpu_done, 1);
}

static void plain_worker(void *arg)
{
    volatile uint32_t x = 0;
    (void)arg;

    for (uint32_t i = 0; i < 1000; i++) {
        x += i;
    }
    (void)x;
    Kalango_TaskYield();
}

void test_fpu_run(void)
{
    RUN_TEST(test_fpu_context_switch);
}

void test_fpu_context_switch(void)
{
    TaskSettings fpu_task = {
        .priority = 5,
        .stack_size = 1024,
        .function = fpu_worker,
        .arg = NULL,
    };
    TaskSettings plain_task = {
        .priority = 5,
        .stack_size = 512,
        .function = plain_worker,
        .arg = NULL,
    };

    fpu_done = Kalango_SemaphoreCreate(0, 1);
    fpu_result = 0.0f;

    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&plain_task));
    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&fpu_task));

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(fpu_done, 30));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, fpu_result);
}
