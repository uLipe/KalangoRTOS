#include <semaphore.h>
#if CONFIG_NOOF_SEMAPHORES > 0

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

    Semaphore * s = (Semaphore *)semaphore;

    CoreSchedulingSuspend();
    if(s->count) {
        IrqDisable();
        s->count--;
        IrqEnable();

        CoreSchedulingResume();
        return kSuccess;
    }

    if(timeout != KERNEL_NO_WAIT) {
        TaskControBlock *task = CoreGetCurrentTask();
        CoreMakeTaskPending(task, TASK_STATE_PEND_SEMAPHORE, &s->pending_tasks);
        AddTimeout(&task->timeout, timeout, NULL, NULL, true, &s->pending_tasks);
        CheckReschedule();
        CoreSchedulingSuspend();

        //Still locked or expired:
        if(task->timeout.expired) {
            CoreSchedulingResume();
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

    Semaphore * s = (Semaphore *)semaphore;

    CoreSchedulingSuspend();

    IrqDisable();
    s->count += count;
    (s->count > s->limit) ? s->count = s->limit : s->count;
    IrqEnable();


    if(NothingToSched(&s->pending_tasks)) {
        CoreSchedulingResume();
        return kSuccess;
    } else {
        IrqDisable();
        s->count--;
        IrqEnable();

        CoreUnpendNextTask(&s->pending_tasks);

        //Not need to reeschedule a new unpended task in a ISR,
        //it will be done a single time after all ISRs
        //get processed 
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