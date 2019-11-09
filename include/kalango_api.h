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
 *  This is the kalang API file, here is a glue file of all
 *  subsystems of kalango RTOS found in a single place, 
 *  your application should call those functions to interact
 *  with kalango kernel instead calling directly a particular
 *  subsystem function.
 *  
 *  Some definition regard of types, can be found on kernel_types.h
 *  file, the other headers are intended to kernel internal use.
 */

/**
 * @fn Kalango_CoreStart
 * @brief Starts the kalango kernel and core system
 * @return never returns
 * @note calling this function multiple times result in immediate return
 */ 
static inline KernelResult Kalango_CoreStart() {
    return CoreStart();
}

/**
 * @fn Kalango_GetTicksPerSecond
 * @brief Get current ticks per second
 * @return Ticks per second
 * @note This function depends on kernel configuration
 */ 
static inline uint32_t Kalango_GetTicksPerSecond() {
    return GetTicksPerSecond();
}

/**
 * @fn Kalango_GetCurrentTicks
 * @brief Return the current elapsed ticks since kernel started
 * @return value of ticks after the kernel started
 */ 
static inline uint32_t Kalango_GetCurrentTicks() {
    return GetCurrentTicks();
}

/**
 * @fn Kalango_Sleep
 * @brief Put current thread to sleep
 * @param ticks - ticks to keep current thread in sleep
 * @return kSuccess when task wakes up
 */ 
static inline KernelResult Kalango_Sleep(uint32_t ticks) {
    return Sleep(ticks);
}

/**
 * @fn Kalango_TaskCreate
 * @brief Creates a new task and put it into the ready list
 * @param settings - structure that contains initial settings of task
 * @return a unique task_id on succesful creation
 * @note if the created task has the highest priority, it will put in execution
 *       instead of placed only on ready list;
 * @note refer TaskSettings contents on kernel_types.h
 */ 
static inline TaskId Kalango_TaskCreate(TaskSettings *settings) {
    return TaskCreate(settings);
}

/**
 * @fn Kalango_TaskSuspend
 * @brief Suspends the execution of an task
 * @param 
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TaskSuspend(TaskId task_id) {
    return TaskSuspend(task_id);
}

/**
 * @fn Kalango_TaskResume
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TaskResume(TaskId task_id) {
    return TaskResume(task_id);
}

/**
 * @fn Kalango_TaskDelete
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TaskDelete(TaskId task_id) {
    return TaskDelete(task_id);
}

/**
 * @fn Kalango_TaskSetPriority
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
 * @fn Kalango_TaskGetPriority
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline uint32_t Kalango_TaskGetPriority(TaskId task_id) {
    return TaskGetPriority(task_id);
}

/**
 * @fn Kalango_TaskYield
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TaskYield() {
    return TaskYield();
}


/**
 * @fn Kalango_SemaphoreCreate
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
 * @fn Kalango_SemaphoreTake
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
 * @fn Kalango_SemaphoreGive
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
 * @fn Kalango_SemaphoreDelete
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_SemaphoreDelete (SemaphoreId semaphore) {
    return SemaphoreDelete(semaphore);
}


/**
 * @fn Kalango_MutexCreate
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline MutexId Kalango_MutexCreate() {
    return MutexCreate();
}

/**
 * @fn Kalango_MutexTryLock
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_MutexTryLock(MutexId mutex) {
    return MutexTryLock(mutex);
}

/**
 * @fn Kalango_MutexLock
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_MutexLock(MutexId mutex, uint32_t timeout) {
    return MutexLock(mutex, timeout);
}

/**
 * @fn Kalango_MutexUnlock
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_MutexUnlock(MutexId mutex) {
    return MutexUnlock(mutex);
}

/**
 * @fn Kalango_MutexDelete
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_MutexDelete(MutexId mutex) {
    return MutexDelete(mutex);
}


/**
 * @fn Kalango_QueueCreate
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
 * @fn Kalango_QueueInsert
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
 * @fn Kalango_QueuePeek
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
 * @fn Kalango_QueueRemove
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
 * @fn Kalango_QueueDelete
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_QueueDelete(QueueId queue) {
    return QueueDelete(queue);
}

/**
 * @fn Kalango_TimerCreate
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
 * @fn Kalango_TimerStart
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TimerStart(TimerId timer) {
    return TimerStart(timer);
}

/**
 * @fn Kalango_TimerStop
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TimerStop(TimerId timer) {
    return TimerStop(timer);
}

/**
 * @fn Kalango_TimerSetValues
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
 * @fn Kalango_TimerDelete
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_TimerDelete(TimerId timer) {
    return TimerDelete(timer);
}


/**
 * @fn Kalango_IrqEnable
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_IrqEnable() {
    return IrqEnable();
}

/**
 * @fn Kalango_IrqDisable
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_IrqDisable() {
    return IrqDisable();
}

/**
 * @fn Kalango_IrqInstallHandler
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_IrqInstallHandler(uint32_t handler, int32_t irq_number, uint32_t priority) {
    return IrqInstallHandler(handler, irq_number, priority);
}

/**
 * @fn Kalango_IrqEnableHandler
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_IrqEnableHandler(int32_t irq_number) {
    return IrqEnableHandler(irq_number);
}

/**
 * @fn Kalango_IrqDisableHandler
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_IrqDisableHandler(int32_t irq_number) {
    return IrqDisableHandler(irq_number);
}

/**
 * @fn Kalango_IrqEnter
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_IrqEnter() {
    return IrqEnter();
}

/**
 * @fn Kalango_IrqLeave
 * @brief
 * @param
 * @return
 * @note
 */ 
static inline KernelResult Kalango_IrqLeave() {
    return IrqLeave();
}