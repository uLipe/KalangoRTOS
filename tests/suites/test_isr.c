#include "unity.h"
#include "test_suites.h"
#include "test_declarations.h"

#include <platform_qemu.h>

#include <KalangoRTOS/kalango_api.h>

static SemaphoreId isr_sem;
static volatile uint32_t isr_gave;

static void isr_hook(void)
{
    isr_gave = 1;
    Kalango_SemaphoreGive(isr_sem, 1);
}

void test_isr_run(void)
{
    platform_putchar('I');
    RUN_TEST(test_isr_semaphore_give);
}

void test_isr_semaphore_give(void)
{
    isr_sem = Kalango_SemaphoreCreate(0, 1);
    isr_gave = 0;

    TEST_ASSERT_EQUAL(kSuccess, Kalango_IrqRequest(16U, isr_hook, 0U));
    TEST_ASSERT_EQUAL(kSuccess, Kalango_IrqEnableHandler(16U));

    platform_trigger_test_irq();

    TEST_ASSERT_EQUAL(kSuccess, Kalango_SemaphoreTake(isr_sem, 20));
    TEST_ASSERT_EQUAL_UINT32(1, isr_gave);

    (void)Kalango_IrqDetach(16U);
}
