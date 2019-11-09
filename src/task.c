#include <task.h>

#if CONFIG_NOOF_TASKS < 2 
    #error "The kernel needs at least 1 user plus 1 kernel tcbs, check config file"
#endif    

TaskId TaskCreate(TaskSettings *settings) {
    ASSERT_KERNEL(settings, NULL);
    ASSERT_KERNEL(settings->function,NULL);
    ASSERT_KERNEL(settings->stack_area, NULL);
    ASSERT_KERNEL(settings->stack_size, NULL);
    ASSERT_KERNEL(settings->priority <= CONFIG_PRIORITY_LEVELS, NULL);
    ASSERT_KERNEL(settings->priority >= 0, NULL);

    CoreInit();

    CoreSchedulingSuspend();
    TaskControBlock *task = AllocateTaskObject();

    if(task == NULL) {
        CoreSchedulingResume();
        return NULL;   
    }

    task->entry_point = settings->function;
    task->priority = settings->priority;
    task->arg1 = settings->arg;
    task->stackpointer = settings->stack_area;
    task->stack_size = settings->stack_size;

    task->asserted_signals = 0;
    task->state = 0;
    
    KernelResult r = ArchNewTask(task, task->stackpointer, task->stack_size);
    if(r != kSuccess) {
        FreeTaskObject(task);
        CoreSchedulingResume();
        return NULL;
    }

    CoreMakeTaskReady(task);
    CheckReschedule();

    return ((TaskId)task);
}

KernelResult TaskSuspend(TaskId task_id) {
    ASSERT_PARAM(task_id);

    TaskControBlock *task = (TaskControBlock *)task_id;
    CoreSchedulingSuspend();

    if(task->state & TASK_STATE_SUPENDED) {
        CoreSchedulingResume();
        return kErrorTaskAlreadySuspended;
    }

    CoreMakeTaskPending(task, TASK_STATE_SUPENDED, NULL);
    return (CheckReschedule());
}

KernelResult TaskResume(TaskId task_id) {
    ASSERT_PARAM(task_id);

    TaskControBlock *task = (TaskControBlock *)task_id;
    CoreSchedulingSuspend();

    if((task->state & TASK_STATE_SUPENDED) == 0) {
        CoreSchedulingResume();
        return kErrorTaskAlreadyResumed;
    }

    CoreMakeTaskReady(task);
    return (CheckReschedule());
}

KernelResult TaskDelete(TaskId task_id) {
    ASSERT_PARAM(task_id);

    TaskControBlock *task = (TaskControBlock *)task_id;
    CoreSchedulingSuspend();

    CoreMakeTaskPending(task, TASK_STATE_TERMINATED, NULL);
    return (CheckReschedule());
}

uint32_t TaskSetPriority(TaskId task_id, uint32_t new_priority) {
    ASSERT_KERNEL(task_id, 0xFFFFFFFF);
    ASSERT_KERNEL(new_priority < CONFIG_PRIORITY_LEVELS, 0xFFFFFFFF);

    TaskControBlock *task = (TaskControBlock *)task_id;
    CoreSchedulingSuspend();

    uint32_t old_prio = task->priority;
    
    IrqDisable();
    task->priority = new_priority;
    IrqEnable();

    if(!task->state) {
        //Force ready task to be moved to correct place on ready queue;
        //Suspended task will be moved once the pending condition terminates
        CoreMakeTaskReady(task);
    }

    CheckReschedule();
    return (old_prio);
}

uint32_t TaskGetPriority(TaskId task_id) {
    ASSERT_PARAM(task_id);
    TaskControBlock *task = (TaskControBlock *)task_id;
  
    return(task->priority);
}

KernelResult TaskYield() {
    TaskControBlock *task = CoreGetCurrentTask();
    CoreMakeTaskReady(task);
    CheckReschedule();
    return (kSuccess);
}

