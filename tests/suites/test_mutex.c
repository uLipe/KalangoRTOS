#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <KalangoRTOS/kalango_api.h>

static SemaphoreId lock_done;
static MutexId shared_mutex;

static void mutex_blocker_task(void *arg)
{
    (void)arg;

    TEST_ASSERT_EQUAL(kSuccess, Kalango_MutexLock(shared_mutex, KERNEL_WAIT_FOREVER));
    test_sleep_ticks(20);
    Kalango_MutexUnlock(shared_mutex);
    Kalango_SemaphoreGive(lock_done, 1);
}

static void mutex_holder_task(void *arg)
{
    (void)arg;

    TEST_ASSERT_EQUAL(kSuccess, Kalango_MutexLock(shared_mutex, KERNEL_WAIT_FOREVER));
    test_sleep_ticks(2);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_MutexUnlock(shared_mutex));
    Kalango_SemaphoreGive(lock_done, 1);
}

static void mutex_waiter_task(void *arg)
{
    (void)arg;

    TEST_ASSERT_EQUAL(kSuccess, Kalango_MutexLock(shared_mutex, KERNEL_WAIT_FOREVER));
    Kalango_SemaphoreGive(lock_done, 1);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_MutexUnlock(shared_mutex));
}

void test_mutex_run(void)
{
    RUN_TEST(test_mutex_trylock_and_unlock);
    RUN_TEST(test_mutex_blocks_second_task);
    RUN_TEST(test_mutex_wrong_owner_unlock);
}

void test_mutex_trylock_and_unlock(void)
{
    MutexId m = Kalango_MutexCreate();

    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL(kSuccess, Kalango_MutexTryLock(m));
    TEST_ASSERT_EQUAL(kStatusMutexAlreadyTaken, Kalango_MutexTryLock(m));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_MutexUnlock(m));
}

void test_mutex_blocks_second_task(void)
{
    TaskSettings holder = {
        .priority = 4,
        .stack_size = 512,
        .function = mutex_holder_task,
        .arg = NULL,
    };
    TaskSettings waiter = {
        .priority = 4,
        .stack_size = 512,
        .function = mutex_waiter_task,
        .arg = NULL,
    };

    lock_done = Kalango_SemaphoreCreate(0, 2);
    shared_mutex = Kalango_MutexCreate();
    TEST_ASSERT_NOT_NULL(shared_mutex);

    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&holder));
    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&waiter));

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(lock_done, 20));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(lock_done, 20));
}

void test_mutex_wrong_owner_unlock(void)
{
    MutexId m = Kalango_MutexCreate();
    TaskSettings settings = {
        .priority = 4,
        .stack_size = 512,
        .function = mutex_blocker_task,
        .arg = NULL,
    };

    lock_done = Kalango_SemaphoreCreate(0, 1);
    shared_mutex = m;

    TEST_ASSERT_NOT_NULL(Kalango_TaskCreate(&settings));
    test_sleep_ticks(2);
    TEST_ASSERT_EQUAL(kErrorInvalidMutexOwner, Kalango_MutexUnlock(m));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(lock_done, 30));
}
