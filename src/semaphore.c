#include <semaphore.h>
#if CONFIG_ENABLE_SEMAPHORES > 0

SemaphoreId SemaphoreCreate(uint32_t initial, uint32_t limit) {
    ASSERT_KERNEL(limit, NULL);

    CoreInit();
    CoreSchedulingSuspend();

    Semaphore *semaphore = AllocateSemaphoreObject();
    if(semaphore == NULL) {
        CoreSchedulingResume();
        return NULL;
    }


    semaphore->count = initial;
    semaphore->limit = limit;
    KernelResult r = CoreInitializeTaskList(&semaphore->pending_tasks);

    if(r != kSuccess) {
        FreeSemaphoreObject(semaphore);
        CoreSchedulingResume();
        return NULL;
    }
    
    CoreSchedulingResume();
    return ((SemaphoreId)semaphore);
}

KernelResult SemaphoreTake(SemaphoreId semaphore, uint32_t timeout) {
    ASSERT_PARAM(semaphore);
    ASSERT_KERNEL(!IsInsideIsr(), kErrorInsideIsr);

    CoreSchedulingSuspend();
    Semaphore * s = (Semaphore *)semaphore;

    if(s->count) {
        s->count--;
        CoreSchedulingResume();
        return kSuccess;
    }

    if(timeout != KERNEL_NO_WAIT) {
        TaskControBlock *task = CoreGetCurrentTask();
        CoreMakeTaskPending(task, TASK_STATE_PEND_SEMAPHORE, &s->pending_tasks);
        AddTimeout(&task->timeout, timeout, NULL, NULL, true, &s->pending_tasks);
        CheckReschedule();

        //Still locked or expired:
        if(task->timeout.expired) {
            return kErrorTimeout;
        }
        return kSuccess;
    
    } else {
        CoreSchedulingResume();
        return kStatusSemaphoreUnavailable;
    }
}

KernelResult SemaphoreGive(SemaphoreId semaphore, uint32_t count) {
    ASSERT_PARAM(semaphore);
    ASSERT_PARAM(count);

    CoreSchedulingSuspend();
    Semaphore * s = (Semaphore *)semaphore;

    s->count += count;
    (s->count > s->limit) ? s->count = s->limit : s->count;

    if(NothingToSched(&s->pending_tasks)) {
        CoreSchedulingResume();
        return kSuccess;
    } else {
        
        if(s->count > 0) {
            s->count--;
        }

        CoreUnpendNextTask(&s->pending_tasks);
 
        if(IsInsideIsr()) {
            CoreSchedulingResume();
            return kSuccess;
        } else {
            return CheckReschedule();
        }
    }
}

KernelResult SemaphoreDelete (SemaphoreId semaphore) {
    ASSERT_PARAM(semaphore);
    ASSERT_KERNEL(!IsInsideIsr(), kErrorInsideIsr);
    Semaphore * s = (Semaphore *)semaphore;

    CoreSchedulingSuspend();
    CoreMakeAllTasksReady(&s->pending_tasks);
    FreeSemaphoreObject(s);
    return CheckReschedule();
}

#endif