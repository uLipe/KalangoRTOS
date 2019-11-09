/**
 *  The  Kalango project, a always experimental RTOS
 */
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

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_CoreStart() {
    return CoreStart();
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline uint32_t Kalango_GetTicksPerSecond() {
    return GetTicksPerSecond();
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline uint32_t Kalango_GetCurrentTicks() {
    return GetCurrentTicks();
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_Sleep(uint32_t ticks) {
    return Sleep(ticks);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline TaskId Kalango_TaskCreate(TaskSettings *settings) {
    return TaskCreate(settings);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TaskSuspend(TaskId task_id) {
    return TaskSuspend(task_id);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TaskResume(TaskId task_id) {
    return TaskResume(task_id);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TaskDelete(TaskId task_id) {
    return TaskDelete(task_id);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline uint32_t Kalango_TaskSetPriority(TaskId task_id, 
                                            uint32_t new_priority) {
    return TaskSetPriority(task_id, new_priority);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline uint32_t Kalango_TaskGetPriority(TaskId task_id) {
    return TaskGetPriority(task_id);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TaskYield() {
    return TaskYield();
}


/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline SemaphoreId Kalango_SemaphoreCreate(uint32_t initial, 
                                                uint32_t limit) {
    return SemaphoreCreate(initial, limit);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_SemaphoreTake(SemaphoreId semaphore, 
                                                uint32_t timeout) {
    return SemaphoreTake(semaphore, timeout); 
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_SemaphoreGive(SemaphoreId semaphore, 
                                                uint32_t count) {
    return SemaphoreGive(semaphore, count);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_SemaphoreDelete (SemaphoreId semaphore) {
    return SemaphoreDelete(semaphore);
}


/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline MutexId Kalango_MutexCreate() {
    return MutexCreate();
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_MutexTryLock(MutexId mutex) {
    return MutexTryLock(mutex);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_MutexLock(MutexId mutex, uint32_t timeout) {
    return MutexLock(mutex, timeout);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_MutexUnlock(MutexId mutex) {
    return MutexUnlock(mutex);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_MutexDelete(MutexId mutex) {
    return MutexDelete(mutex);
}


/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline QueueId Kalango_QueueCreate(uint32_t noof_slots, 
                                        uint32_t slot_size, 
                                        uint8_t *buffer) {
    return QueueCreate(noof_slots, slot_size, buffer);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_QueueInsert(QueueId queue, 
                                            void *data, 
                                            uint32_t data_size, 
                                            uint32_t timeout) {
    return QueueInsert(queue, data, data_size, timeout);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_QueuePeek(QueueId queue, 
                                            void *data, 
                                            uint32_t *data_size, 
                                            uint32_t timeout) {
    return QueuePeek(queue, data, data_size, timeout);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_QueueRemove(QueueId queue, 
                                            void *data, 
                                            uint32_t *data_size, 
                                            uint32_t timeout) {
    return QueueRemove(queue, data, data_size, timeout);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_QueueDelete(QueueId queue) {
    return QueueDelete(queue);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline TimerId Kalango_TimerCreate(TimerCallback callback, 
                                        uint32_t expiry_time, 
                                        uint32_t period_time) {
    return TimerCreate(callback, expiry_time, period_time);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TimerStart(TimerId timer) {
    return TimerStart(timer);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TimerStop(TimerId timer) {
    return TimerStop(timer);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TimerSetValues(TimerId timer, 
                                                uint32_t expiry_time, 
                                                uint32_t period_time) {
    return TimerSetValues(timer, expiry_time, period_time);
}

/**
 * @fn
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TimerDelete(TimerId timer) {
    return TimerDelete(timer);
}


static inline KernelResult Kalango_IrqEnable() {
    return IrqEnable();
}

static inline KernelResult Kalango_IrqDisable() {
    return IrqDisable();
}

static inline KernelResult Kalango_IrqInstallHandler(uint32_t handler, int32_t irq_number, uint32_t priority) {
    return IrqInstallHandler(handler, irq_number, priority);
}

static inline KernelResult Kalango_IrqEnableHandler(int32_t irq_number) {
    return IrqEnableHandler(irq_number);
}

static inline KernelResult Kalango_IrqDisableHandler(int32_t irq_number) {
    return IrqDisableHandler(irq_number);
}

static inline KernelResult Kalango_IrqEnter() {
    return IrqEnter();
}

static inline KernelResult Kalango_IrqLeave() {
    return IrqLeave();
}