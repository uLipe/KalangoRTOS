#include <core.h>

TaskControBlock *current = NULL;
static TaskControBlock *next_task = NULL;
static TaskPriorityList ready_tasks_list;
static bool initialized = false;
static bool is_running = false;
static TaskId task_idle_id;
static sys_dlist_t tasks_waiting_to_delete = SYS_DLIST_STATIC_INIT(&tasks_waiting_to_delete);

static void IdleTask(void *unused) {
    (void)unused;

    for(;;) {
        //Delete tasks waiting to termination:
        sys_dnode_t *task_node = sys_dlist_peek_head(&tasks_waiting_to_delete);
        if(task_node) {
            TaskControBlock *task = CONTAINER_OF(task_node, TaskControBlock, ready_node);

            IrqDisable();
            sys_dlist_remove(&task->ready_node);
            IrqEnable();

            FreeRawBuffer(task->stackpointer);
            FreeTaskObject(task);
        }
    }
}

KernelResult CoreMakeTaskPending(TaskControBlock * task, uint32_t reason, TaskPriorityList *kobject_pending_list) {
    ASSERT_PARAM(task);
    ASSERT_PARAM(reason);

    IrqDisable();
    sys_dlist_remove(&task->ready_node);
    SchedulerResetPriority(&ready_tasks_list, task->priority);

    task->state = reason;

    if(kobject_pending_list) {
        sys_dlist_append(&kobject_pending_list->task_list[task->priority], &task->ready_node);
        SchedulerSetPriority(kobject_pending_list, task->priority);
    }

    if(reason & TASK_STATE_TERMINATED) {
        sys_dlist_append(&tasks_waiting_to_delete, &task->ready_node);
    }

    IrqEnable();
    return kSuccess;
}

KernelResult CoreUnpendNextTask(TaskPriorityList *kobject_pending_list) {
    ASSERT_PARAM(kobject_pending_list);

    TaskControBlock *task = ScheduleTaskSet(kobject_pending_list);

    if(task) {
        IrqDisable();
        RemoveTimeout(&task->timeout);
        sys_dlist_remove(&task->ready_node);
        SchedulerResetPriority(kobject_pending_list, task->priority);
        IrqEnable();

        return (CoreMakeTaskReady(task));
    } else {
        return kErrorNothingToSchedule;
    }
}

KernelResult CoreMakeTaskReady(TaskControBlock * task) {
    ASSERT_PARAM(task);

    IrqDisable();

    task->state = TASK_STATE_READY;
    sys_dlist_append(&ready_tasks_list.task_list[task->priority], &task->ready_node);
    SchedulerSetPriority(&ready_tasks_list, task->priority);

    IrqEnable();

    return kSuccess;
}

KernelResult CoreMakeAllTasksReady(TaskPriorityList *tasks) {
    ASSERT_PARAM(tasks);

    IrqDisable();

    while(!NothingToSched(tasks)) {
        CoreUnpendNextTask(tasks);
    }
 
    IrqEnable();

    return kSuccess;
}

TaskControBlock * CoreTaskSwitch() { 
    current = next_task;
    return next_task;
}

KernelResult CoreManageRoundRobin() {
    return SchedulerDoRoundRobin(&ready_tasks_list);
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

    TaskSettings settings;
    settings.arg = NULL;
    settings.function = IdleTask;
    settings.priority = 0;
    settings.stack_size = CONFIG_IDLE_TASK_STACK_SIZE;
    task_idle_id = TaskCreate(&settings);
    
    ASSERT_KERNEL(task_idle_id != NULL, kErrorInvalidParam);

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
