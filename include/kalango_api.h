/**
 *  The  Kalango project, a always experimental RTOS
 */
#pragma once
/**
 *  This is the kalang API file, here is a glue file of all
 *  subsystems of kalango RTOS to be found in a single place, 
 *  your application should call those functions to interact
 *  with kalango kernel instead calling directly a particular
 *  subsystem function.
 *  
 *  Some definition regarding of types, can be found on kernel_types.h
 *  file, the other headers are intended to kernel internal use.
 */

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
 * @fn Kalango_GetCurrentTaskId
 * @brief Return the Id of current executing task
 * @return Id of the current task
 */ 
static inline TaskId Kalango_GetCurrentTaskId() {
    return ((TaskId) CoreGetCurrentTask());
}

/**
 * @fn Kalango_Sleep
 * @brief Put current thread to sleep
 * @param ticks - ticks to keep current thread in sleep
 * @return kSuccess when task wakes up
 * @note calling this function from a ISR results in immediate
 *      return and error
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
 * @note calling this function from a ISR results in immediate
 *      return and error 
 */ 
static inline TaskId Kalango_TaskCreate(TaskSettings *settings) {
    return TaskCreate(settings);
}

/**
 * @fn Kalango_TaskSuspend
 * @brief Suspends the execution of an task
 * @param task_id - id of target task to suspend
 * @return kSuccess on succesful suspension
 * @note Suspension does not support nesting, that is it, 
 *       if this function suspend a already suspended task it will 
 *       return error
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_TaskSuspend(TaskId task_id) {
    return TaskSuspend(task_id);
}

/**
 * @fn Kalango_TaskResume
 * @brief Resume the execution of an task
 * @param task_id - id of target task to resume
 * @return kSuccess on task placed on ready list
 * @note calling resume for a ready task results in error
 */ 
static inline KernelResult Kalango_TaskResume(TaskId task_id) {
    return TaskResume(task_id);
}

/**
 * @fn Kalango_TaskDelete
 * @brief Terminate a task in execution 
 * @param task_id - id of target task to terminate
 * @return kSuccess on termination
 * @note this function actually does not delete a task, it simply put that 
 *      on a not runnable state, then only calling task create can put it
 *      again on execution.
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_TaskDelete(TaskId task_id) {
    return TaskDelete(task_id);
}

/**
 * @fn Kalango_TaskSetPriority
 * @brief Changes the priority of a task
 * @param task_id - id of target task to change the priority
 * @param new_priority - new priority of target task
 * @return old priority of target task or -1 on error
 * @note If priority changing results on target task to be the highest one,
 *      it will placed in execution immediately if is not already.
 */ 
static inline uint32_t Kalango_TaskSetPriority(TaskId task_id, 
                                            uint32_t new_priority) {
    return TaskSetPriority(task_id, new_priority);
}

/**
 * @fn Kalango_TaskGetPriority
 * @brief Gets the priority of an task
 * @param task_id - id of desired priority task
 * @return priority of that task
 */ 
static inline uint32_t Kalango_TaskGetPriority(TaskId task_id) {
    return TaskGetPriority(task_id);
}

/**
 * @fn Kalango_TaskYield
 * @brief Voluntary releases the CPU for the next task on FIFO
 * @return kSuccess on succesful yielding
 * @note The yield will only occur if there are, at least 2 tasks of same priority
 *      on the ready list, otherwise calling this function will be ignored
 * @note calling this function from a ISR results in immediate
 *      return and error 
 */ 
static inline KernelResult Kalango_TaskYield() {
    return TaskYield();
}


/**
 * @fn Kalango_SemaphoreCreate
 * @brief Creates a counting or binary semaphore
 * @param initial - initial count available on that semaphore
 * @param limit - maximum counting allowed to this semaphore
 * @return A unique semaphore id bonded to created object
 * @note To create a binary semaphore makes initial equal to 0 and
 *      the limit equal to 1
 */ 
static inline SemaphoreId Kalango_SemaphoreCreate(uint32_t initial, 
                                                uint32_t limit) {
    return SemaphoreCreate(initial, limit);
}

/**
 * @fn Kalango_SemaphoreTake
 * @brief Takes a semaphore, blocks if not available
 * @param   semaphre - id of desired semaphore
 * @param   timeout - amount of timeout to wait if it is not available
 * @return  kSuccess on semaphore took, kTimeout if not after wait
 * @note passing KERNEL_WAIT_FOREVER as timeout will make the task block until semaphore
 *      make it available
 * @note passing KERNEL_NO_WAIT as timeout causes immediate return if semaphore is not available
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_SemaphoreTake(SemaphoreId semaphore, 
                                                uint32_t timeout) {
    return SemaphoreTake(semaphore, timeout); 
}

/**
 * @fn Kalango_SemaphoreGive
 * @brief Makes a semaphore available by some counts
 * @param semaphore - id of target semaphore
 * @param count - desired counts of releasing
 * @return kSuccess on succesful giving
 * @note passing count above the semaphore limit will cause the 
 *      semaphore to make all its counts available
 */ 
static inline KernelResult Kalango_SemaphoreGive(SemaphoreId semaphore, 
                                                uint32_t count) {
    return SemaphoreGive(semaphore, count);
}

/**
 * @fn Kalango_SemaphoreDelete
 * @brief Deletes a semaphore 
 * @param semaphore - id of semaphore to be deleted
 * @return kSuccess on succesful deletion
 * @note Deleting a semaphore will put all the waiting tasks for it in a ready state
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_SemaphoreDelete (SemaphoreId semaphore) {
    return SemaphoreDelete(semaphore);
}


/**
 * @fn Kalango_MutexCreate
 * @brief Creates a mutual exclusion semaphore
 * @return A unique id bonded to Mutex object
 * @note New mutexes starts all in not locked state
 */ 
static inline MutexId Kalango_MutexCreate() {
    return MutexCreate();
}

/**
 * @fn Kalango_MutexTryLock
 * @brief Tries to lock / acquire the mutex
 * @param mutex - id to the desired mutex
 * @return kSuccess on mutex locked
 * @note on fail to acquire a mutex this function will return
 *      immediately
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_MutexTryLock(MutexId mutex) {
    return MutexTryLock(mutex);
}

/**
 * @fn Kalango_MutexLock
 * @brief Try to acquire a mutex, blocks if it is not available
 * @param mutex - id for the desired mutex
 * @param timeout - timeout to wait for a locked mutex in ticks
 * @return kSuccess on mutex acquired / kErrorTimeout if waiting time expires
 * @note passing KERNEL_WAIT_FOREVER as timeout will make the task block until mutex
 *      make it available
 * @note passing KERNEL_NO_WAIT as timeout causes immediate return if mutex is not available
 * @note Mutexes prevent deadlock by being recursive, if a particular task lock a mutex by
 *      multiple times it will nested, mutexes need to be unlocked by the same amount to 
 *      be actually freed.
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_MutexLock(MutexId mutex, uint32_t timeout) {
    return MutexLock(mutex, timeout);
}

/**
 * @fn Kalango_MutexUnlock
 * @brief Unlocks a previously locked mutex
 * @param mutex - id of target mutex
 * @return kSuccess on succesful unlocking
 * @note Only the mutex owner can unlock the mutex
 * @note Unlock a recursively locked mutex will return kErrorMutexALready taken
 *      until all recursive lockings will be undone
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_MutexUnlock(MutexId mutex) {
    return MutexUnlock(mutex);
}

/**
 * @fn Kalango_MutexDelete
 * @brief Deletes a mutex
 * @param mutex - id of desired mutex
 * @return kSucess on succesful deletion
 * @note Deleting a mutex will put all the waiting tasks for it on ready list
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_MutexDelete(MutexId mutex) {
    return MutexDelete(mutex);
}


/**
 * @fn Kalango_QueueCreate
 * @brief Created a message queue
 * @param noof_slots - number of slots of this queue
 * @param slot_size - the size of each slot of message queue
 * @param buffer - user allocated area where slots will be put
 * @return A unique id bonded to created queue
 * @note slot_size is in bytes
 * @note the user allocated buffer should have a size in bytes equal to:
 *      noof_slots * slot_size, size smaller than this value will result
 *      in crashes when queue starts to became full
 */ 
static inline QueueId Kalango_QueueCreate(uint32_t noof_slots, 
                                        uint32_t slot_size, 
                                        uint8_t *buffer) {
    return QueueCreate(noof_slots, slot_size, buffer);
}

/**
 * @fn Kalango_QueueInsert
 * @brief Inserts a new slot on tail of queue, blocks if it is full
 * @param queue - id of target queue
 * @param data - the data to be inserted on the slot
 * @param data_size - size of data to be inserted
 * @param timeout - ticks to wait until a slot of queue become free
 * @return kSuccess on data copied, kErrorTimeout on waiting time expiration
 * @note data_size should be the same value of slot_size in bytes, lesser
 *      values can be passed but must be handled by user application, 
 *      passing larger values than slot will return on error
 * @note passing KERNEL_WAIT_FOREVER as timeout will make the task block until 
 *        a slot becomes it available
 * @note passing KERNEL_NO_WAIT as timeout causes immediate return if queue is full
 * @note calling this function from a ISR is only allowed when passing
 *      KERNEL_NO_WAIT as timeout parameter, other cases return immediately
 *      with error. 
 */ 
static inline KernelResult Kalango_QueueInsert(QueueId queue, 
                                            void *data, 
                                            uint32_t data_size, 
                                            uint32_t timeout) {
    return QueueInsert(queue, data, data_size, timeout);
}

/**
 * @fn Kalango_QueuePeek
 * @brief Receives data from head of the queue, blocks if it is empty
 * @param queue - id of target queue
 * @param data - user allocated area to store data received from queue
 * @param data_size - user pointer to store current slot size 
 * @param timeout - ticks to wait until at least one slot arrives on queue
 * @return kSuccess on data copied, kErrorTimeout on waiting time expiration
 * @note peek a queue will receive the data, but will not update the queue head
 * @note data_size pointer parameter is actually not in use, can be NULL
 * @note passing KERNEL_WAIT_FOREVER as timeout will make the task block until 
 *        a slot becomes it available
 * @note passing KERNEL_NO_WAIT as timeout causes immediate return if queue is empty
 * @note calling this function from a ISR is only allowed when passing
 *      KERNEL_NO_WAIT as timeout parameter, other cases return immediately
 *      with error.
 */ 
static inline KernelResult Kalango_QueuePeek(QueueId queue, 
                                            void *data, 
                                            uint32_t *data_size, 
                                            uint32_t timeout) {
    return QueuePeek(queue, data, data_size, timeout);
}

/**
 * @fn Kalango_QueueRemove
 * @brief Receives data from head of the queue, update it or, blocks if it is empty
 * @param queue - id of target queue
 * @param data - user allocated area to store data received from queue
 * @param data_size - user pointer to store current slot size 
 * @param timeout - ticks to wait until at least one slot arrives on queue
 * @return kSuccess on data copied, kErrorTimeout on waiting time expiration
 * @note data_size pointer parameter is actually not in use, can be NULL
 * @note passing KERNEL_WAIT_FOREVER as timeout will make the task block until 
 *        a slot becomes it available
 * @note passing KERNEL_NO_WAIT as timeout causes immediate return if queue is empty
 * @note calling this function from a ISR is only allowed when passing
 *      KERNEL_NO_WAIT as timeout parameter, other cases return immediately
 *      with error. 
 */ 
static inline KernelResult Kalango_QueueRemove(QueueId queue, 
                                            void *data, 
                                            uint32_t *data_size, 
                                            uint32_t timeout) {
    return QueueRemove(queue, data, data_size, timeout);
}

/**
 * @fn Kalango_QueueDelete
 * @brief Delete a message queue
 * @param queue - id to target queue
 * @return kSuccess on succesful deletion
 * @note Deleting a queue will put all waiting tasks on ready list, no valid data will
 *      be inserted or received
 * @note calling this function from a ISR results in immediate
 *      return and error
 */ 
static inline KernelResult Kalango_QueueDelete(QueueId queue) {
    return QueueDelete(queue);
}

/**
 * @fn Kalango_TimerCreate
 * @brief Creates a new timer 
 * @param callback - function invoked by timer on expiration
 * @param expiry_time - ticks to count before throw the callback
 * @param  period_time - if not 0 defines ticks to periodically throw the callback
 * @param  user_data - pointer to a user defined data if will passed to callback as param
 * @return A unique id bonded to the created timer
 * @note after created the timer is not running yet.
 * @note callbacks thrown by timers actually executes on ISR context so
 *      avoid processing inside them.
 */ 
static inline TimerId Kalango_TimerCreate(TimerCallback callback, 
                                        uint32_t expiry_time, 
                                        uint32_t period_time,
                                        void* user_data) {
    return TimerCreate(callback, expiry_time, period_time, user_data);
}

/**
 * @fn Kalango_TimerStart
 * @brief Starts or restarts a timer
 * @param timer - id of desired timer to start
 * @return kSuccess on timer started to count
 * @note calling this function for a running timer implies on 
 *      reset its counting to 0 (restart)
 */ 
static inline KernelResult Kalango_TimerStart(TimerId timer) {
    return TimerStart(timer);
}

/**
 * @fn Kalango_TimerStop
 * @brief Stops a timer to count
 * @param timer - id of desired timer to stop
 * @return kSuccess on if timer stopped
 */ 
static inline KernelResult Kalango_TimerStop(TimerId timer) {
    return TimerStop(timer);
}

/**
 * @fn Kalango_TimerSetValues
 * @brief Sets the expiration and period of a timer
 * @param timer - id of desired timer
 * @param expiry_time - ticks to count before throw the callback
 * @param  period_time - if not 0 defines ticks to periodically throw the callback
 * @return kSuccess on succesful setting
 * @note calling this function to running timer will cause its stopping before 
 *      the new values are set, the set timer needs to be restarted by 
 *      calling Kalango_TimerStart() again.
 */ 
static inline KernelResult Kalango_TimerSetValues(TimerId timer, 
                                                uint32_t expiry_time, 
                                                uint32_t period_time) {
    return TimerSetValues(timer, expiry_time, period_time);
}

/**
 * @fn Kalango_TimerDelete
 * @brief Deletes a timer 
 * @param timer - id of timer to delete
 * @return kSuccess on succesful deletion
 * @note calling this function by a running timer will cause it 
 *      to be stopped before its being deleted.
 */ 
static inline KernelResult Kalango_TimerDelete(TimerId timer) {
    return TimerDelete(timer);
}


/**
 * @fn Kalango_IrqEnable
 * @brief Globally enables the cpu interrups
 * @return always success
 * @note If IrqDisable was called recursively before by multiple times
 *      this function should be called by the same amount to actually
 *      enables the interrupts
 */ 
static inline KernelResult Kalango_IrqEnable() {
    return IrqEnable();
}

/**
 * @fn Kalango_IrqDisable
 * @brief Disables globally the CPU interrupts
 * @return Always success
 * @note Calling this function recursively will cause the irq nesting
 *      refer Kalango_IrqEnable() to know how to enable the interrupts
 *      in this case.
 */ 
static inline KernelResult Kalango_IrqDisable() {
    return IrqDisable();
}

/**
 * @fn Kalango_IrqInstallHandler
 * @brief Installs a ISR callback handler on a IRQ number slot
 * @param handler - address of interrupt callback handler
 * @param irq_number - number of slot to install this isr
 * @param priority - if supported by platform, the priority level off this ISR
 * @return kSuccess on succesfull instalation
 * @note installed handlers are enabled by default
 * @note priority parameter may not be supported by your platform
 * @note irq_number parameter needs to match of current platform soc
 * @note be cautious some irqs are used internally by the kernel, trying to install irq handlers on those 
 *       slots may return in error
 */ 
static inline KernelResult Kalango_IrqInstallHandler(uint32_t handler, int32_t irq_number, uint32_t priority) {
    return IrqInstallHandler(handler, irq_number, priority);
}

/**
 * @fn Kalango_IrqEnableHandler
 * @brief Enables a ISR on particular slot
 * @param irq_number - number of slot to enable ISR
 * @return kSuccess if enabling was succesful
 */ 
static inline KernelResult Kalango_IrqEnableHandler(int32_t irq_number) {
    return IrqEnableHandler(irq_number);
}

/**
 * @fn Kalango_IrqDisableHandler
 * @brief Disable ISR on a particular slot
 * @param irq_number - number of slot to disable ISR
 * @return kSuccess on succesful disabling
 */ 
static inline KernelResult Kalango_IrqDisableHandler(int32_t irq_number) {
    return IrqDisableHandler(irq_number);
}

/**
 * @fn Kalango_IrqEnter
 * @brief Starts a ISR safe region
 * @return always kSuccess
 * @note This function MUST be placed before any other instruction on a ISR if 
 *      application wants to use RTOS functions.
 */ 
static inline KernelResult Kalango_IrqEnter() {
    return IrqEnter();
}

/**
 * @fn Kalango_IrqLeave
 * @brief Ends the ISR safe place 
 * @return always kSuccess
 * @note Place this function as last instruction of an ISR, combined
 *      witth Kalango_IrqEnter() the user can call RTOS functions 
 *      safely
 */ 
static inline KernelResult Kalango_IrqLeave() {
    return IrqLeave();
}