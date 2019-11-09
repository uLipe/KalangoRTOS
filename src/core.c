#include <core.h>

TaskControBlock *current = NULL;
static TaskControBlock *next_task = NULL;
static TaskPriorityList ready_tasks_list;
static bool initialized = false;
static bool is_running = false;

KernelResult CoreMakeTaskPending(TaskControBlock * task, uint32_t reason, TaskPriorityList *kobject_pending_list) {
    ASSERT_PARAM(task);
    ASSERT_PARAM(reason);

    IrqDisable();
    sys_dlist_remove(&task->ready_node);
    IrqEnable();

    task->state = reason;

    if(kobject_pending_list) {
        IrqDisable();
        sys_dlist_append(&kobject_pending_list->task_list[task->priority], &task->pending_node);
        IrqEnable();
    }
    return kSuccess;
}

KernelResult CoreMakeTaskReady(TaskControBlock * task) {
    ASSERT_PARAM(task);

    IrqDisable();
    sys_dlist_remove(&task->pending_node);
    IrqEnable();

    task->state = TASK_STATE_READY;

    IrqDisable();
    SchedulerSetPriority(&ready_tasks_list, task->priority);
    sys_dlist_append(&ready_tasks_list.task_list[task->priority], &task->ready_node);
    IrqEnable();

    return kSuccess;
}

KernelResult CoreMakeAllTasksReady(TaskPriorityList *tasks) {
    ASSERT_PARAM(tasks);

    for(uint32_t i =0 ; i < CONFIG_PRIORITY_LEVELS; i++) {
        while(!sys_dlist_is_empty(&tasks->task_list[i])) {
            sys_dnode_t *node = sys_dlist_peek_head(&tasks->task_list[i]);
            TaskControBlock *task = CONTAINER_OF(node, TaskControBlock, pending_node);
            CoreMakeTaskReady(task);
        }

        SchedulerResetPriority(tasks, i);
    }

    return kSuccess;
}

TaskControBlock * CoreTaskSwitch() { 
    current = next_task;
    return next_task;
}

KernelResult CheckReschedule() {

    CoreSchedulingResume();

    //We should not reeschedule if scheduler is still locked:
    if(IsSchedulerLocked(&ready_tasks_list)) {
        return kStatusSchedLocked;
    }

    next_task = ScheduleTaskSet(&ready_tasks_list);
    ASSERT_KERNEL(next_task, kErrorInvalidKernelState);

    //Shall we switch the context?:
    if(next_task != current && (is_running)) {

        if(IsInsideIsr()) {
            return ArchSwitchFromInterrupt();
        } else {
            return ArchSwitchFromTask();
        }
    }
    return kSuccess;
}

KernelResult CoreInitializeTaskList(TaskPriorityList *list) {
    ASSERT_PARAM(list);
    SchedulerInitTaskPriorityList(list);
    return kSuccess;
}

TaskControBlock * CoreGetCurrentTask() {
    return current;
}

KernelResult CoreInit() {
    
    if(initialized) {
        return kSuccess;
    }

    IrqDisable();
    initialized = true;
    InitializeObjectPools();
    CoreInitializeTaskList(&ready_tasks_list);
    IrqEnable();
    return kSuccess;
}

KernelResult CoreStart() {
    if(is_running) {
        return kSuccess;
    }

    IrqDisable();

    CoreInit();

#if CONFIG_USE_PLATFORM_INIT > 0
    PlatformInit(NULL);
#endif 
   
    ArchInitializeSpecifics();
    
    next_task = ScheduleTaskSet(&ready_tasks_list);
    ASSERT_KERNEL(next_task, kErrorInvalidKernelState);
    
    current = next_task;
    IrqEnable();
    ArchStartKernel();

    return kSuccess;
}

bool IsCoreRunning() {
    return is_running;
}

void CoreSetRunning() {
    
    if(!IsInsideIsr()) {
        return;
    }

    is_running = true;
}

KernelResult CoreSchedulingSuspend() {
    return SchedulerLock(&ready_tasks_list);
}

KernelResult CoreSchedulingResume() {
    return SchedulerUnlock(&ready_tasks_list);
}
