#include <clock.h>

static sys_dlist_t timeout_list = SYS_DLIST_STATIC_INIT(&timeout_list);
static uint32_t tick_counter = 0;

#if CONFIG_NOOF_TIMERS > 0
static KernelResult HandleExpiredTimers(sys_dlist_t *expired_list) {

    sys_dnode_t *next = sys_dlist_peek_head(expired_list);

    while(next) {
        Timeout *timeout = CONTAINER_OF(next, Timeout, timed_node);
        Timer *timer = CONTAINER_OF(timeout, Timer, timeout);

        if(timeout->timeout_callback) {
            timeout->timeout_callback(timeout->user_data);
        }

        if(timer->periodic) {
            AddTimeout(&timer->timeout, timer->period_time, timer->callback, timer->user_data, false, NULL);
        } else {
            timer->expired = true;
            timer->running = false;
        }

        next = sys_dlist_peek_next(expired_list, next);
    }

    return kSuccess;
}
#endif 

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
    AddTimeout(&current->timeout, ticks, NULL, NULL, true, NULL);

    return (CheckReschedule());
}

KernelResult ClockStep (uint32_t ticks) {
    if(!IsCoreRunning()) {
        return kErrorInvalidKernelState;
    }

    ASSERT_KERNEL(IsInsideIsr(), kErrorTimeout);
    sys_dlist_t expired_list;
    sys_dnode_t *next = sys_dlist_peek_head(&timeout_list);

    sys_dlist_init(&expired_list);
    tick_counter += ticks;

    while(next) {
        Timeout *timeout = CONTAINER_OF(next, Timeout, timed_node);
        if(timeout->next_wakeup_tick <= tick_counter) {
            timeout->expired = true;
            if(!timeout->is_task) {
                sys_dlist_prepend(&expired_list, &timeout->timed_node);
            } else {
                TaskControBlock *timed_out = CONTAINER_OF(timeout, TaskControBlock,timeout);
                if(timeout->bonded_list != NULL) {
                    SchedulerResetPriority(timeout->bonded_list, timed_out->priority);
                }
                CoreMakeTaskReady(timed_out);
            }
            next = sys_dlist_peek_next(&timeout_list, next);
            sys_dlist_remove(&timeout->timed_node);

            
        } else {
            next = sys_dlist_peek_next(&timeout_list, next);
        }
    }
    

#if CONFIG_NOOF_TIMERS > 0
    if(!sys_dlist_is_empty(&expired_list)) {
        return HandleExpiredTimers(&expired_list);
    }
#endif

    return kSuccess;
}

KernelResult AddTimeout(Timeout *timeout, 
                        uint32_t value, 
                        TimerCallback timeout_callback, 
                        void *user_data, 
                        bool is_task,
                        TaskPriorityList *optional_list_to_bind) {
    ASSERT_PARAM(timeout);
    ASSERT_PARAM(value);
    
    timeout->is_task = is_task;
    timeout->next_wakeup_tick = tick_counter + value;
    timeout->timeout_callback = timeout_callback;
    timeout->user_data = user_data;
    timeout->expired = false;
    if(optional_list_to_bind != NULL) {
        timeout->bonded_list = optional_list_to_bind;
    }

    IrqDisable();
    sys_dlist_append(&timeout_list, &timeout->timed_node);
    IrqEnable();

    return kSuccess;
}

KernelResult RemoveTimeout(Timeout *timeout) {
    ASSERT_PARAM(timeout);

    IrqDisable();
    sys_dlist_remove(&timeout->timed_node);
    IrqEnable();

    return kSuccess;
}