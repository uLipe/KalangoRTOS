#include <clock.h>

static sys_dlist_t timeout_list = SYS_DLIST_STATIC_INIT(&timeout_list);
static uint32_t tick_counter = 0;

static KernelResult HandleExpiredTimers(Timeout *timeout) {
    
    Timer *timer = CONTAINER_OF(timeout, Timer, timeout);
    
    if(timeout->timeout_callback) {
        timeout->timeout_callback(timeout->user_data);
    }

    if(timer->periodic) {
        //Periodic timer, keeps into the list but shift the expiration count:
        timeout->next_wakeup_tick = tick_counter + timer->period_time;
        timer->expired = false;
        timer->running = true;
    } else {
        timer->expired = true;
        timer->running = false;
        sys_dlist_remove(&timeout->timed_node);
    }
    
    return kSuccess;
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
    AddTimeout(&current->timeout, ticks, NULL, NULL, true, NULL);

    return (CheckReschedule());
}

KernelResult ClockStep (uint32_t ticks) {
    if(!IsCoreRunning()) {
        return kErrorInvalidKernelState;
    }

    ASSERT_KERNEL(ArchInIsr(), kErrorInvalidKernelState);

    tick_counter += ticks;

    CoreManageRoundRobin();
    
    sys_dnode_t *next;
    sys_dnode_t *tmp;

    SYS_DLIST_FOR_EACH_NODE_SAFE(&timeout_list, next, tmp) {

        Timeout *timeout = CONTAINER_OF(next, Timeout, timed_node);
        if(timeout->next_wakeup_tick == tick_counter) {

            timeout->expired = true;

            if(!timeout->is_task) {
                HandleExpiredTimers(timeout);
            } else {
                sys_dlist_remove(next);

                TaskControBlock *timed_out = CONTAINER_OF(timeout, TaskControBlock,timeout);
                if(timeout->bonded_list != NULL) {
                    SchedulerResetPriority(timeout->bonded_list, timed_out->priority);
                }
                CoreMakeTaskReady(timed_out);
            }

        }
    }

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
    
    if(value == KERNEL_WAIT_FOREVER){
        return kSuccess;
    }

    timeout->is_task = is_task;
    timeout->next_wakeup_tick = tick_counter + value;
    timeout->timeout_callback = timeout_callback;
    timeout->user_data = user_data;
    timeout->expired = false;
    if(optional_list_to_bind != NULL) {
        timeout->bonded_list = optional_list_to_bind;
    }

    ArchCriticalSectionEnter();
    sys_dlist_append(&timeout_list, &timeout->timed_node);
    ArchCriticalSectionExit();

    return kSuccess;
}

KernelResult RemoveTimeout(Timeout *timeout) {
    ASSERT_PARAM(timeout);

    ArchCriticalSectionEnter();
    sys_dlist_remove(&timeout->timed_node);
    ArchCriticalSectionExit();

    return kSuccess;
}