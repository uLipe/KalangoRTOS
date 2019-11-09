#include <mutex.h>

#if CONFIG_NOOF_MUTEXES > 0

MutexId MutexCreate(){

    CoreInit();
    CoreSchedulingSuspend();
    Mutex *mutex = AllocateMutexObject();

    if(mutex == NULL) {
        CoreSchedulingResume();
        return NULL;
    }

    mutex->old_priority = 0;
    mutex->owned = false;
    mutex->recursive_taking_count = 0;
    
    KernelResult r = CoreInitializeTaskList(&mutex->pending_tasks);
    if(r != kSuccess) {
        FreeMutexObject(mutex);
        CoreSchedulingResume();
        return NULL;
    }

    CoreSchedulingResume();
    return((MutexId)mutex);
}

KernelResult MutexTryLock(MutexId mutex)  {
    ASSERT_PARAM(mutex);
    ASSERT_KERNEL(!IsInsideIsr(), kErrorInsideIsr);

    Mutex *m = (Mutex *)mutex;

    CoreSchedulingSuspend();

    if(m->owned) {
        CoreSchedulingResume();
        return kStatusMutexAlreadyTaken;
    }


    IrqDisable();
    m->owned = true;
    IrqEnable();

    if(m->recursive_taking_count < 0xFFFFFFFF)
        m->recursive_taking_count++;

    TaskControBlock *task = CoreGetCurrentTask();
    
    //Raise priority
    if(TaskGetPriority(task) < CONFIG_MUTEX_CEIL_PRIORITY) {
        m->old_priority = TaskSetPriority(task, CONFIG_MUTEX_CEIL_PRIORITY);
    } else {
        //Dont bump the priority if it already higher than mutex priority
        m->old_priority = TaskGetPriority(task);
    }
    
    return CheckReschedule();
}

KernelResult MutexLock(MutexId mutex, uint32_t timeout) {
    ASSERT_PARAM(mutex);
    ASSERT_KERNEL(!IsInsideIsr(), kErrorInsideIsr);

    Mutex *m = (Mutex *)mutex;

    CoreSchedulingSuspend();

    if(!m->owned) {
        IrqDisable();
        m->owned = true;
        IrqEnable();

        if(m->recursive_taking_count < 0xFFFFFFFF)
            m->recursive_taking_count++;

        TaskControBlock *task = CoreGetCurrentTask();
        
        //Raise priority
        if(TaskGetPriority((TaskId)task) < CONFIG_MUTEX_CEIL_PRIORITY) {
            m->old_priority = TaskSetPriority((TaskId)task, CONFIG_MUTEX_CEIL_PRIORITY);
        } else {
            //Dont bump the priority if it already higher than mutex priority
            m->old_priority = TaskGetPriority((TaskId)task);
        }
        return kSuccess;
    }   

    if(timeout != KERNEL_NO_WAIT) {
        TaskControBlock *task = CoreGetCurrentTask();
        CoreMakeTaskPending(task, TASK_STATE_PEND_MUTEX, &m->pending_tasks);
        AddTimeout(&task->timeout, timeout, NULL, NULL, true, &m->pending_tasks);
        CheckReschedule();
        CoreSchedulingSuspend();

        //Still locked?
        if(task->timeout.expired) {
            CoreSchedulingResume();
            return kErrorTimeout;
        } else {
            return kSuccess;
        }

    } else {
        CoreSchedulingResume();
        return kStatusMutexAlreadyTaken;
    }
}

KernelResult MutexUnlock(MutexId mutex) {
    ASSERT_PARAM(mutex);
    Mutex *m = (Mutex *)mutex;

    CoreSchedulingSuspend();
    TaskControBlock *current = CoreGetCurrentTask();

    if(m->recursive_taking_count) {
        IrqDisable();
        m->recursive_taking_count--;
        IrqEnable();
    }

    if(m->recursive_taking_count != 0) {
        CoreSchedulingResume();
        return kStatusMutexAlreadyTaken;
    }

 
   if(NothingToSched(&m->pending_tasks)) {
        IrqDisable();
        m->owned = false;
        IrqEnable();

        m->old_priority = TaskSetPriority((TaskId)current, m->old_priority);
        CoreSchedulingResume();
        return kSuccess;
    }

    TaskControBlock *task = ScheduleTaskSet(&m->pending_tasks);

    //Bumps the priority of next pending task:
    if(TaskGetPriority((TaskId)task) < CONFIG_MUTEX_CEIL_PRIORITY) {
        m->old_priority = TaskSetPriority((TaskId)task, CONFIG_MUTEX_CEIL_PRIORITY);
    } else {
        //Dont bump the priority if it already higher than mutex priority
        m->old_priority = TaskGetPriority((TaskId)task);
    }
    
    if(m->recursive_taking_count < 0xFFFFFFFF)
        m->recursive_taking_count++;

    RemoveTimeout(&task->timeout);
    CoreMakeTaskReady(task);
    m->old_priority = TaskSetPriority((TaskId)current, m->old_priority);

    return CheckReschedule();
}

KernelResult MutexDelete(MutexId mutex) {
    ASSERT_PARAM(mutex);
    Mutex *m = (Mutex *)mutex;

    CoreSchedulingSuspend();
    CoreMakeAllTasksReady(&m->pending_tasks);
    FreeMutexObject(m);
    return CheckReschedule();
}

#endif