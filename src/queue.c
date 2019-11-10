#include <queue.h>
#if CONFIG_NOOF_QUEUES > 0

QueueId QueueCreate(uint32_t noof_slots, uint32_t slot_size, uint8_t *buffer) {
    ASSERT_KERNEL(noof_slots, NULL);
    ASSERT_KERNEL(slot_size, NULL);
    ASSERT_KERNEL(buffer, NULL);

    CoreInit();
    CoreSchedulingSuspend();

    Queue *queue = AllocateQueueObject();
    if(queue == NULL) {
        CoreSchedulingResume();
        return NULL;
    }

    queue->empty = true;
    queue->noof_slots = noof_slots;
    queue->slot_size = slot_size;
    queue->buffer = buffer;
    queue->full = false;
    queue->head = 0;
    queue->tail = 0;
    queue->available_slots = noof_slots;

    KernelResult r = CoreInitializeTaskList(&queue->reader_tasks_pending);
    if(r != kSuccess) {
        FreeQueueObject(queue);
        CoreSchedulingResume();
        return NULL;
    }
    
    r = CoreInitializeTaskList(&queue->writer_tasks_pending);
    if(r != kSuccess) {
        FreeQueueObject(queue);
        CoreSchedulingResume();
        return NULL;
    }

    CoreSchedulingResume();
    return ((QueueId)queue);
}

KernelResult QueueInsert(QueueId queue, void *data, uint32_t data_size, uint32_t timeout) {
    ASSERT_PARAM(queue);
    ASSERT_PARAM(data);

    Queue *q = (Queue *)queue;
    CoreSchedulingSuspend();

    if(data_size != q->slot_size) {
        CoreSchedulingResume();
        return kErrorInvalidParam;
    }

    if(!q->full) {

        uint32_t write_loc = q->tail;
        
        q->empty = false;
        memcpy(&q->buffer[write_loc], data, data_size);

        IrqDisable();
        if(q->available_slots)
            q->available_slots--;
        IrqEnable();

        write_loc = ((write_loc + 1) % (q->noof_slots)) * q->slot_size;
        q->tail = write_loc;

        if(!q->available_slots){
            q->full = true;
        }

        if(NothingToSched(&q->reader_tasks_pending)) {
            CoreSchedulingResume();
            return kSuccess;
        } else {

            CoreUnpendNextTask(&q->reader_tasks_pending);
            return CheckReschedule();
        }
    }

    if(timeout == KERNEL_NO_WAIT) {
        CoreSchedulingResume();
        return kErrorBufferFull;
    }

    if(IsInsideIsr()) {
        CoreSchedulingResume();
        return kErrorInsideIsr;
    }

    TaskControBlock *task = CoreGetCurrentTask();
    CoreMakeTaskPending(task, TASK_STATE_PEND_QUEUE, &q->writer_tasks_pending);
    AddTimeout(&task->timeout, timeout, NULL, NULL, true, &q->writer_tasks_pending);
    CheckReschedule();

    CoreSchedulingSuspend();

    if(task->timeout.expired) {
        CoreSchedulingResume();
        return kErrorTimeout;
    } else {
        uint32_t write_loc = q->tail;
        
        q->empty = false;
        memcpy(&q->buffer[write_loc], data, data_size);

        IrqDisable();
        if(q->available_slots)
            q->available_slots--;
        IrqEnable();

        write_loc = ((write_loc + 1) % (q->noof_slots)) * q->slot_size;
        q->tail = write_loc;

        if(!q->available_slots){
            q->full = true;
        }

        CoreSchedulingResume();
        return kSuccess;        
    }
}

KernelResult QueuePeek(QueueId queue, void *data, uint32_t *data_size, uint32_t timeout) {
    ASSERT_PARAM(queue);
    ASSERT_PARAM(data);
    (void)data_size;

    Queue *q = (Queue*)queue;

    CoreSchedulingSuspend();
    
    if(!q->empty) {
        uint32_t read_loc = q->head;
        memcpy(data, &q->buffer[read_loc], q->slot_size);
        CoreSchedulingResume();
        return kSuccess;
    }


    if(timeout == KERNEL_NO_WAIT) {
        CoreSchedulingResume();
        return kErrorBufferEmpty;
    }

    if(IsInsideIsr()) {
        CoreSchedulingResume();
        return kErrorInsideIsr;
    }

    TaskControBlock *task = CoreGetCurrentTask();
    CoreMakeTaskPending(task, TASK_STATE_PEND_QUEUE, &q->reader_tasks_pending);
    AddTimeout(&task->timeout, timeout, NULL, NULL, true, &q->reader_tasks_pending);
    CheckReschedule();

    CoreSchedulingSuspend();

    if(task->timeout.expired) {
        CoreSchedulingResume();
        return kErrorTimeout;
    } else {
        uint32_t read_loc = q->head;
        memcpy(data, &q->buffer[read_loc], q->slot_size);
        CoreSchedulingResume();
        return kSuccess;
    }
}

KernelResult QueueRemove(QueueId queue, void *data, uint32_t *data_size, uint32_t timeout) {
    ASSERT_PARAM(queue);
    ASSERT_PARAM(data);
    (void)data_size;

    Queue *q = (Queue*)queue;

    CoreSchedulingSuspend();

    if(!q->empty) {
        uint32_t read_loc = q->head;
        
        q->full = false;
        memcpy(data, &q->buffer[read_loc], q->slot_size);

        read_loc = ((read_loc + 1) % q->slot_size) * q->slot_size;

        IrqDisable();
        if(q->available_slots < 0xFFFFFFFF)
            q->available_slots++;
        q->head = read_loc;
        IrqEnable();
     
        if(q->available_slots >= q->noof_slots) {
            q->available_slots = q->noof_slots;
            q->empty = true;
        }

        if(NothingToSched(&q->writer_tasks_pending)) {
            CoreSchedulingResume();
            return kSuccess;
        } else {

            CoreUnpendNextTask(&q->writer_tasks_pending);
            return CheckReschedule();
        }

    }

    if(timeout == KERNEL_NO_WAIT) {
        CoreSchedulingResume();
        return kErrorBufferEmpty;
    }

    if(IsInsideIsr()) {
        CoreSchedulingResume();
        return kErrorInsideIsr;
    }

    TaskControBlock *task = CoreGetCurrentTask();
    CoreMakeTaskPending(task, TASK_STATE_PEND_QUEUE, &q->reader_tasks_pending);
    AddTimeout(&task->timeout, timeout, NULL, NULL, true, &q->reader_tasks_pending);
    CheckReschedule();

    CoreSchedulingSuspend();

    if(task->timeout.expired) {
        CoreSchedulingResume();
        return kErrorTimeout;
    } else {

        uint32_t read_loc = q->head;
        
        q->full = false;
        memcpy(data, &q->buffer[read_loc], q->slot_size);

        read_loc = ((read_loc + 1) % q->slot_size) * q->slot_size;

        IrqDisable();
        if(q->available_slots < 0xFFFFFFFF)
            q->available_slots++;
        q->head = read_loc;
        IrqEnable();
     
        if(q->available_slots >= q->noof_slots) {
            q->available_slots = q->noof_slots;
            q->empty = true;
        }

        CoreSchedulingResume();
        return kSuccess;
    }
}

KernelResult QueueDelete(QueueId queue) {
    ASSERT_PARAM(queue);

    Queue *q = (Queue *)queue;

    CoreSchedulingSuspend();
    CoreMakeAllTasksReady(&q->writer_tasks_pending);
    CoreMakeAllTasksReady(&q->reader_tasks_pending);
    FreeQueueObject(q);

    return  CheckReschedule();
}

#endif