#include <KalangoRTOS/core.h>
#include <KalangoRTOS/kalango_config_internal.h>

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

            ArchCriticalSectionEnter();
            sys_dlist_remove(&task->ready_node);
            ArchCriticalSectionExit();

            FreeRawBuffer(task->stackpointer);
            FreeTaskObject(task);
        }
    }
}

KernelResult CoreMakeTaskPending(TaskControBlock * task, uint32_t reason, WaitQueue *wq){
    ASSERT_PARAM(task);
    ASSERT_PARAM(reason);

    ArchCriticalSectionEnter();
    sys_dlist_remove(&task->ready_node);
    SchedulerResetPriority(&ready_tasks_list, task->priority);

    task->state = reason;

    if(wq) {
        sys_dlist_append(&wq->waiters, &task->ready_node);
    }

    if(reason & TASK_STATE_TERMINATED) {
        sys_dlist_append(&tasks_waiting_to_delete, &task->ready_node);
    }

    ArchCriticalSectionExit();
    return kSuccess;
}

KernelResult CoreUnpendNextTask(WaitQueue *wq) {
    ASSERT_PARAM(wq);

    TaskControBlock *task;
    sys_dnode_t *node =  sys_dlist_peek_head(&wq->waiters);

    if(node == NULL) {
       return kErrorNothingToSchedule;
    }

    task = CONTAINER_OF(node, TaskControBlock, ready_node);
    ArchCriticalSectionEnter();
    RemoveTimeout(&task->timeout);
    sys_dlist_remove(&task->ready_node);
    ArchCriticalSectionExit();

    return (CoreMakeTaskReady(task));
}

KernelResult CoreMakeTaskReady(TaskControBlock * task) {
    ASSERT_PARAM(task);

    ArchCriticalSectionEnter();

    task->state = TASK_STATE_READY;
    sys_dlist_append(&ready_tasks_list.task_list[task->priority], &task->ready_node);
    SchedulerSetPriority(&ready_tasks_list, task->priority);

    ArchCriticalSectionExit();

    return kSuccess;
}

KernelResult CoreMakeAllTasksReady(WaitQueue *wq) {
    ASSERT_PARAM(wq);

    KernelResult result;

    ArchCriticalSectionEnter();

    do {
        result = CoreUnpendNextTask(wq);
    } while (result != kErrorNothingToSchedule);

    ArchCriticalSectionExit();

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
        return ArchYield();
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

    ArchCriticalSectionEnter();
    initialized = true;
    InitializeObjectPools();
    CoreInitializeTaskList(&ready_tasks_list);
    ArchCriticalSectionExit();
    return kSuccess;
}

KernelResult CoreStart() {
    if(is_running) {
        return kSuccess;
    }

    ArchCriticalSectionEnter();

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
    ArchCriticalSectionExit();
    ArchStartKernel();

    return kSuccess;
}

bool IsCoreRunning() {
    return is_running;
}

void CoreSetRunning() {

#if (CONFIG_ARCH_ARM_V7M > 0)
    if(!ArchInIsr()) {
        return;
    }
#endif

    is_running = true;
}

KernelResult CoreSchedulingSuspend() {
    return SchedulerLock(&ready_tasks_list);
}

KernelResult CoreSchedulingResume() {
    return SchedulerUnlock(&ready_tasks_list);
}

KernelResult CoreInitWaitQueue(WaitQueue *wq) {
    ASSERT_PARAM(wq);
    
    ArchCriticalSectionEnter();
    sys_dlist_init(&wq->waiters);
    ArchCriticalSectionExit();

    return kSuccess;
}

TaskControBlock * CorePeekWaitQueue(WaitQueue *wq) {
    ASSERT_KERNEL(wq, NULL);
    
    sys_dnode_t *node;

    ArchCriticalSectionEnter();
    node = sys_dlist_peek_head(&wq->waiters);
    ArchCriticalSectionExit();

    if(node == NULL) {
        return NULL;
    }

    return (CONTAINER_OF(node, TaskControBlock, ready_node));
}