#pragma once

#include <KalangoRTOS/kalango_config_internal.h>
#include <KalangoRTOS/priority_queue.h>


typedef struct {
    sys_dlist_t task_list[CONFIG_PRIORITY_LEVELS];
    uint32_t ready_task_bitmap;
    uint32_t lock_level;
}TaskPriorityList;

typedef struct {
    sys_dlist_t waiters;
}WaitQueue;

struct Timeout_s;

typedef int (*TimeoutCallback)(struct Timeout_s *timeout);

typedef struct Timeout_s {
    TimeoutCallback timeout_callback;
    uint32_t next_wakeup_tick;
    bool expired;
    bool invalid;
    struct heap_node node;
}Timeout;

typedef struct {
    uint8_t *stackpointer;
    uint32_t stack_size;
    void (*entry_point) (void *);
    void *arg1;
    uint32_t priority;
    uint32_t state;
    Timeout timeout;
    sys_dnode_t ready_node;
} TaskControBlock;

typedef struct {
    uint32_t count;
    uint32_t limit;
    WaitQueue pending_tasks;
}Semaphore;

typedef struct {
    bool owned;
    void *owner;
    uint32_t recursive_taking_count;
    uint32_t old_priority;
    WaitQueue pending_tasks;
}Mutex;

typedef struct {
    void (*callback) (void *);
    void *user_data;
    bool periodic;
    bool expired;
    bool running;
    uint32_t expiry_time;
    uint32_t period_time;
    Timeout timeout;
}Timer;

typedef struct {
    uint8_t *buffer;
    uint32_t tail;
    uint32_t head;
    uint32_t slot_size;
    uint32_t noof_slots;
    uint32_t available_slots;
    bool full;
    bool empty;
    WaitQueue writer_tasks_pending;
    WaitQueue reader_tasks_pending;
} Queue;
