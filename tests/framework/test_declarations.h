#pragma once

void test_core_ticks_increment(void);
void test_core_heap_reports_free_bytes(void);
void test_core_current_task_not_null(void);

void test_task_create_returns_valid_id(void);
void test_task_yield_runs_other_task(void);
void test_task_suspend_and_resume(void);
void test_task_set_get_priority(void);
void test_task_delete_defers_cleanup(void);

void test_semaphore_create_take_give(void);
void test_semaphore_no_wait_unavailable(void);
void test_semaphore_timeout(void);
void test_semaphore_limit_cap(void);

void test_mutex_trylock_and_unlock(void);
void test_mutex_blocks_second_task(void);
void test_mutex_wrong_owner_unlock(void);

void test_queue_insert_remove(void);
void test_queue_peek(void);
void test_queue_full_no_wait(void);
void test_queue_empty_no_wait(void);

void test_timer_one_shot(void);
void test_timer_periodic(void);
void test_timer_stop(void);

void test_sched_higher_priority_preempts(void);
void test_sched_round_robin_same_priority(void);

void test_isr_semaphore_give(void);

void test_edge_null_task_suspend(void);
void test_edge_sleep_zero(void);
void test_edge_semaphore_null(void);

void test_fpu_context_switch(void);

void test_softirq_trigger_calls_handler(void);
void test_softirq_handler_receives_data(void);
void test_softirq_invalid_vector_rejected(void);
void test_softirq_multiple_vectors_dispatched(void);
void test_softirq_no_handler_no_crash(void);
void test_softirq_same_vector_coalesces(void);
