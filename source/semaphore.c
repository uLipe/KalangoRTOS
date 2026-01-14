#include <KalangoRTOS/semaphore.h>
#include <KalangoRTOS/kalango_config_internal.h>

#if CONFIG_ENABLE_SEMAPHORES > 0

static int SemaphoreHandleTimeout(Timeout* t) {
    TaskControBlock *wake_task = CONTAINER_OF(t, TaskControBlock,timeout);
    CoreMakeTaskReady(wake_task);
    return 0;
}


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
    KernelResult r = CoreInitWaitQueue(&semaphore->pending_tasks);

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
    ASSERT_KERNEL(!ArchInIsr(), kErrorInsideIsr);

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
        AddTimeout(&task->timeout, timeout, SemaphoreHandleTimeout);
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
    KernelResult result;

        //If called from ISR, requires a IRQ safe block
    if(ArchInIsr()) {
        if(!ArchGetIsrNesting()){
            return kErrorInvalidKernelState;
        }
    }

    CoreSchedulingSuspend();
    Semaphore * s = (Semaphore *)semaphore;

    s->count += count;
    (s->count > s->limit) ? s->count = s->limit : s->count;

    result = CoreUnpendNextTask(&s->pending_tasks);

    if(result == kErrorNothingToSchedule) {
        CoreSchedulingResume();
        return kSuccess;            
    }

    if(s->count > 0) {
        s->count--;
    }

    if(ArchInIsr()) {
        CoreSchedulingResume();
        return kSuccess;
    } else {
        return CheckReschedule();
    }
}

KernelResult SemaphoreDelete (SemaphoreId semaphore) {
    ASSERT_PARAM(semaphore);
    ASSERT_KERNEL(!ArchInIsr(), kErrorInsideIsr);
    Semaphore * s = (Semaphore *)semaphore;

    CoreSchedulingSuspend();
    CoreMakeAllTasksReady(&s->pending_tasks);
    FreeSemaphoreObject(s);
    return CheckReschedule();
}

#endif