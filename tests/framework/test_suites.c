#include "test_suites.h"
#include <platform_qemu.h>

void test_sleep_ticks(uint32_t ticks)
{
    Kalango_Sleep(ticks);
}

void RunAllTests(void)
{
    test_core_run();
    test_tasks_run();
    test_semaphore_run();
    test_mutex_run();
    test_queue_run();
    test_timer_run();
    test_sched_run();
    platform_putchar('X');
    test_isr_run();
    test_edge_run();
#if CONFIG_HAS_FLOAT > 0
    test_fpu_run();
#endif
    test_softirq_run();
}
