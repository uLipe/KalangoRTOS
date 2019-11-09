#pragma once

#include <kernel_types.h>
#include <core.h>
#include <semaphore.h>
#include <task.h>
#include <queue.h>
#include <clock.h>
#include <sched.h>
#include <timer.h>
#include <mutex.h>
#include <irq.h>

static inline KernelResult Kalango_CoreStart() {
    return CoreStart();
}

static inline uint32_t Kalango_GetTicksPerSecond() {
    return GetTicksPerSecond();
}

static inline uint32_t Kalango_GetCurrentTicks() {
    return GetCurrentTicks();
}

static inline KernelResult Kalango_Sleep(uint32_t ticks) {
    return Sleep(ticks);
}

static inline TaskId Kalango_TaskCreate(TaskSettings *settings) {
    return TaskCreate(settings);
}

static inline KernelResult Kalango_TaskSuspend(TaskId task_id) {
    return TaskSuspend(task_id);
}

static inline KernelResult Kalango_TaskResume(TaskId task_id) {
    return TaskResume(task_id);
}

static inline KernelResult Kalango_TaskDelete(TaskId task_id) {
    return TaskDelete(task_id);
}

static inline uint32_t Kalango_TaskSetPriority(TaskId task_id, 
                                            uint32_t new_priority) {
    return TaskSetPriority(task_id, new_priority);
}

static inline uint32_t Kalango_TaskGetPriority(TaskId task_id) {
    return TaskGetPriority(task_id);
}

static inline KernelResult Kalango_TaskYield() {
    return TaskYield();
}


static inline SemaphoreId Kalango_SemaphoreCreate(uint32_t initial, 
                                                uint32_t limit) {
    return SemaphoreCreate(initial, limit);
}

static inline KernelResult Kalango_SemaphoreTake(SemaphoreId semaphore, 
                                                uint32_t timeout) {
    return SemaphoreTake(semaphore, timeout); 
}

static inline KernelResult Kalango_SemaphoreGive(SemaphoreId semaphore, 
                                                uint32_t count) {
    return SemaphoreGive(semaphore, count);
}

static inline KernelResult Kalango_SemaphoreDelete (SemaphoreId semaphore) {
    return SemaphoreDelete(semaphore);
}


static inline MutexId Kalango_MutexCreate() {
    return MutexCreate();
}

static inline KernelResult Kalango_MutexTryLock(MutexId mutex) {
    return MutexTryLock(mutex);
}

static inline KernelResult Kalango_MutexLock(MutexId mutex, uint32_t timeout) {
    return MutexLock(mutex, timeout);
}

static inline KernelResult Kalango_MutexUnlock(MutexId mutex) {
    return MutexUnlock(mutex);
}
static inline KernelResult Kalango_MutexDelete(MutexId mutex) {
    return MutexDelete(mutex);
}


static inline QueueId Kalango_QueueCreate(uint32_t noof_slots, 
                                        uint32_t slot_size, 
                                        uint8_t *buffer) {
    return QueueCreate(noof_slots, slot_size, buffer);
}

static inline KernelResult Kalango_QueueInsert(QueueId queue, 
                                            void *data, 
                                            uint32_t data_size, 
                                            uint32_t timeout) {
    return QueueInsert(queue, data, data_size, timeout);
}

static inline KernelResult Kalango_QueuePeek(QueueId queue, 
                                            void *data, 
                                            uint32_t *data_size, 
                                            uint32_t timeout) {
    return QueuePeek(queue, data, data_size, timeout);
}

static inline KernelResult Kalango_QueueRemove(QueueId queue, 
                                            void *data, 
                                            uint32_t *data_size, 
                                            uint32_t timeout) {
    return QueueRemove(queue, data, data_size, timeout);
}

static inline KernelResult Kalango_QueueDelete(QueueId queue) {
    return QueueDelete(queue);
}

static inline TimerId Kalango_TimerCreate(TimerCallback callback, 
                                        uint32_t expiry_time, 
                                        uint32_t period_time) {
    return TimerCreate(callback, expiry_time, period_time);
}

static inline KernelResult Kalango_TimerStart(TimerId timer) {
    return TimerStart(timer);
}

static inline KernelResult Kalango_TimerStop(TimerId timer) {
    return TimerStop(timer);
}

static inline KernelResult Kalango_TimerSetValues(TimerId timer, 
                                                uint32_t expiry_time, 
                                                uint32_t period_time) {
    return TimerSetValues(timer, expiry_time, period_time);
}

static inline KernelResult Kalango_TimerDelete(TimerId timer) {
    return TimerDelete(timer);
}