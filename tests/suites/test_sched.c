#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>
#include <platform_qemu.h>

static SemaphoreId sched_done;
static volatile uint32_t high_ran;
static volatile uint32_t low_ran;
static volatile uint32_t rr_count_a;
static volatile uint32_t rr_count_b;

static void high_prio_task(void *arg)
{
    (void)arg;
    high_ran = 1;
    Kalango_SemaphoreGive(sched_done, 1);
}

static void low_prio_task(void *arg)
{
    (void)arg;
    low_ran = 1;
}

static void rr_task_a(void *arg)
{
    (void)arg;

    if (rr_count_a < 3) {
        rr_count_a++;
    }
    Kalango_TaskYield();
}

static void rr_task_b(void *arg)
{
    (void)arg;

    if (rr_count_b < 3) {
        rr_count_b++;
    }
    Kalango_TaskYield();
}

void test_sched_run(void)
{
    platform_putchar('A');
    RUN_TEST(test_sched_higher_priority_preempts);
    platform_putchar('B');
    RUN_TEST(test_sched_round_robin_same_priority);
    platform_putchar('C');
}

void test_sched_higher_priority_preempts(void)
{
    TaskSettings high = {
        .priority = 10,
        .stack_size = 512,
        .function = high_prio_task,
        .arg = NULL,
    };
    TaskSettings low = {
        .priority = 2,
        .stack_size = 512,
        .function = low_prio_task,
        .arg = NULL,
    };

    sched_done = Kalango_SemaphoreCreate(0, 1);
    high_ran = 0;
    low_ran = 0;

    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&low));
    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&high));

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(sched_done, 20));
    TEST_ASSERT_EQUAL_UINT32(1, high_ran);
}

void test_sched_round_robin_same_priority(void)
{
    TaskSettings a = {
        .priority = 6,
        .stack_size = 512,
        .function = rr_task_a,
        .arg = NULL,
    };
    TaskSettings b = {
        .priority = 6,
        .stack_size = 512,
        .function = rr_task_b,
        .arg = NULL,
    };

    rr_count_a = 0;
    rr_count_b = 0;

    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&a));
    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&b));

    test_sleep_ticks(10);
    TEST_ASSERT_GREATER_THAN(0, (int)rr_count_a);
    TEST_ASSERT_GREATER_THAN(0, (int)rr_count_b);
}
