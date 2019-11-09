#pragma once

#include <KalangoRTOS/kalango_api.h>

void test_sleep_ticks(uint32_t ticks);

void test_core_run(void);
void test_tasks_run(void);
void test_semaphore_run(void);
void test_mutex_run(void);
void test_queue_run(void);
void test_timer_run(void);
void test_sched_run(void);
void test_isr_run(void);
void test_edge_run(void);

#if CONFIG_HAS_FLOAT > 0
void test_fpu_run(void);
#endif

void test_softirq_run(void);

void RunAllTests(void);
