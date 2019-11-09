#include <KalangoRTOS/clock.h>
#include <KalangoRTOS/kalango_config_internal.h>

static int ClockTimeCompare(struct heap_node *a, struct heap_node *b);

static struct priority_queue timeout_list = {
    .head = NULL,
    .tail = NULL,
    .root = NULL,
    .compare = ClockTimeCompare,
};

static uint32_t tick_counter = 0;

static int ClockTimeCompare(struct heap_node *a, struct heap_node *b)
{
    Timeout *timer_a = CONTAINER_OF(a, Timeout, node);
    Timeout *timer_b = CONTAINER_OF(b, Timeout, node);

    if(timer_a->next_wakeup_tick < timer_b->next_wakeup_tick)
        return 1;
    else if (timer_a->next_wakeup_tick > timer_b->next_wakeup_tick)
        return -1;
    
    return -1;
}

static int ClockHandleSleepTasks(Timeout* t) {
    TaskControBlock *wake_task = CONTAINER_OF(t, TaskControBlock,timeout);
    CoreMakeTaskReady(wake_task);
    return 0;
}

uint32_t GetTicksPerSecond() {
    return CONFIG_TICKS_PER_SEC;
}

uint32_t GetCurrentTicks() {
    return tick_counter;
}

KernelResult Sleep(uint32_t ticks) {
    ASSERT_PARAM(ticks);

    CoreSchedulingSuspend();
    TaskControBlock *current = CoreGetCurrentTask();
    CoreMakeTaskPending(current, TASK_STATE_PEND_TIMEOUT, NULL);
    AddTimeout(&current->timeout, ticks, ClockHandleSleepTasks);

    return (CheckReschedule());
}

KernelResult ClockStep (uint32_t ticks) {
    if(!IsCoreRunning()) {
        return kErrorInvalidKernelState;
    }

    ASSERT_KERNEL(ArchInIsr(), kErrorInvalidKernelState);

#if CONFIG_ENABLE_ROUND_ROBIN_SCHED
    CoreManageRoundRobin();
#endif

    tick_counter += ticks;
    int need_reorder = 0;
    Timeout *next_timer = NULL;
    struct heap_node *node = pq_peek(&timeout_list);

    while (node && (next_timer = CONTAINER_OF(node, Timeout, node))->next_wakeup_tick <= tick_counter) {
        pq_pop(&timeout_list);

        if(next_timer->timeout_callback) {
            next_timer->expired = true;
            need_reorder |= next_timer->timeout_callback(next_timer);
        } 

        node = pq_peek(&timeout_list);
    }

    if(need_reorder)
    	pq_reorder(&timeout_list);

    return kSuccess;
}

KernelResult AddTimeout(Timeout *timeout,
                        uint32_t value,
                        TimeoutCallback timeout_callback) {
    ASSERT_PARAM(timeout);
    ASSERT_PARAM(value);
    ASSERT_PARAM(timeout_callback);

    if(value == KERNEL_WAIT_FOREVER){
        timeout->invalid = true;
        timeout->expired = false;
        return kSuccess;
    }

    timeout->invalid = false;
    timeout->expired = false;
    timeout->next_wakeup_tick = tick_counter + value;
    timeout->timeout_callback = timeout_callback;

    ArchCriticalSectionEnter();
    pq_insert(&timeout_list, &timeout->node);
    pq_reorder(&timeout_list);
    ArchCriticalSectionExit();

    return kSuccess;
}

KernelResult RemoveTimeout(Timeout *timeout) {
    ASSERT_PARAM(timeout);

    if(timeout->invalid) {
        return kSuccess;
    } 

    ArchCriticalSectionEnter();
    pq_remove(&timeout_list, &timeout->node);
    pq_reorder(&timeout_list);
    ArchCriticalSectionExit();

    return kSuccess;
}