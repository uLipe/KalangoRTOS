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
    mutex->owner = NULL;
    
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

    m->owned = true;

    if(m->recursive_taking_count < 0xFFFFFFFF)
        m->recursive_taking_count++;

    TaskControBlock *task = CoreGetCurrentTask();
    m->owner = task;

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
        m->owned = true;

        if(m->recursive_taking_count < 0xFFFFFFFF)
            m->recursive_taking_count++;

        TaskControBlock *task = CoreGetCurrentTask();
        m->owner = task;

        //Raise priority
        if(TaskGetPriority((TaskId)task) < CONFIG_MUTEX_CEIL_PRIORITY) {
            m->old_priority = TaskSetPriority((TaskId)task, CONFIG_MUTEX_CEIL_PRIORITY);
        } else {
            //Dont bump the priority if it already higher than mutex priority
            m->old_priority = TaskGetPriority((TaskId)task);
        }

        return CheckReschedule();
    }   

    if(timeout != KERNEL_NO_WAIT) {
        TaskControBlock *task = CoreGetCurrentTask();
        CoreMakeTaskPending(task, TASK_STATE_PEND_MUTEX, &m->pending_tasks);
        AddTimeout(&task->timeout, timeout, NULL, NULL, true, &m->pending_tasks);
        CheckReschedule();

        //Still locked?
        if(task->timeout.expired) {
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
    ASSERT_KERNEL(!IsInsideIsr(), kErrorInsideIsr);

    Mutex *m = (Mutex *)mutex;

    CoreSchedulingSuspend();
    TaskControBlock *current = CoreGetCurrentTask();

    if(current != (TaskControBlock *)m->owner) {
        CoreSchedulingResume();
        return kErrorInvalidMutexOwner;
    }

    if(m->recursive_taking_count) {
        m->recursive_taking_count--;
    }

    if(m->recursive_taking_count != 0) {
        CoreSchedulingResume();
        return kStatusMutexAlreadyTaken;
    }

 
   if(NothingToSched(&m->pending_tasks)) {
        m->owned = false;
        m->owner = NULL;

        m->old_priority = TaskSetPriority((TaskId)current, m->old_priority);
        CheckReschedule();

        return kSuccess;
    }

    TaskControBlock *task = ScheduleTaskSet(&m->pending_tasks);
    m->owner = task;

    //Bumps the priority of next pending task:
    if(TaskGetPriority((TaskId)task) < CONFIG_MUTEX_CEIL_PRIORITY) {
        m->old_priority = TaskSetPriority((TaskId)task, CONFIG_MUTEX_CEIL_PRIORITY);
    } else {
        //Dont bump the priority if it already higher than mutex priority
        m->old_priority = TaskGetPriority((TaskId)task);
    }
    
    if(m->recursive_taking_count < 0xFFFFFFFF)
        m->recursive_taking_count++;

    CoreUnpendNextTask(&m->pending_tasks);
    m->old_priority = TaskSetPriority((TaskId)current, m->old_priority);

    return CheckReschedule();
}

KernelResult MutexDelete(MutexId mutex) {
    ASSERT_PARAM(mutex);
    ASSERT_KERNEL(!IsInsideIsr(), kErrorInsideIsr);
    
    Mutex *m = (Mutex *)mutex;

    CoreSchedulingSuspend();
    CoreMakeAllTasksReady(&m->pending_tasks);
    FreeMutexObject(m);
    return CheckReschedule();
}

#endif