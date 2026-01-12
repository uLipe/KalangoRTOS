/**
 * @file kalango_api.h
 * @brief KalangoRTOS public API (facade).
 *
 * @details
 * This header exposes the functions that an application should use to interact with KalangoRTOS.
 * The intention is that user code calls the `Kalango_*` wrappers instead of directly calling
 * internal subsystems (core/task/semaphore/mutex/queue/timer/clock/scheduler).
 *
 * Public types (TaskId, SemaphoreId, MutexId, QueueId, TimerId, KernelResult, TaskSettings, etc.)
 * are defined in `kernel_types.h`.
 *
 * @note Timeouts accept:
 * - `KERNEL_WAIT_FOREVER` (-1)
 * - `KERNEL_NO_WAIT` (0)
 *
 * @note ISR handling:
 * Some APIs support being called from an ISR **only if** the ISR is wrapped with
 * `Kalango_IrqEnter()` / `Kalango_IrqLeave()` (see @ref kalango_isr).
 *
 * @defgroup kalango_api KalangoRTOS Public API
 * @{
 */

#pragma once

#include <KalangoRTOS/kernel_types.h>

/* Internal headers (ideally hidden from the application in a future refactor).
 * Kept here to preserve current project compatibility. */
#include <KalangoRTOS/core.h>
#include <KalangoRTOS/semaphore.h>
#include <KalangoRTOS/task.h>
#include <KalangoRTOS/queue.h>
#include <KalangoRTOS/clock.h>
#include <KalangoRTOS/sched.h>
#include <KalangoRTOS/timer.h>
#include <KalangoRTOS/mutex.h>
#include <KalangoRTOS/object_pool.h>

/* -------------------------------------------------------------------------- */
/* Core / System                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @defgroup kalango_core Core / System
 * @ingroup kalango_api
 * @brief Kernel start-up and global system information.
 * @{
 */

/**
 * @brief Starts the kernel and begins scheduling.
 *
 * @return KernelResult status.
 *
 * @retval kSuccess Kernel already running or successfully started.
 * @retval kErrorInvalidParam Idle task creation failed (TaskCreate returned NULL).
 * @retval kErrorInvalidKernelState Scheduler produced an invalid next task.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_CoreStart(void) {
    return CoreStart();
}

/**
 * @brief Returns the tick frequency (ticks per second).
 *
 * @return Number of ticks per second.
 *
 * @thread_safety ISR-safe.
 */
static inline uint32_t Kalango_GetTicksPerSecond(void) {
    return GetTicksPerSecond();
}

/**
 * @brief Returns the current system tick count.
 *
 * @return Current tick counter value.
 *
 * @thread_safety ISR-safe.
 */
static inline uint32_t Kalango_GetCurrentTicks(void) {
    return GetCurrentTicks();
}

/**
 * @brief Returns the ID of the currently running task.
 *
 * @return Current task ID.
 *
 * @warning If called before the kernel is running / current task is not set, behavior depends on internals.
 * @thread_safety ISR-safe (returns current task pointer), but typically used in thread context.
 */
static inline TaskId Kalango_GetCurrentTaskId(void) {
    return CoreGetCurrentTask();
}

/**
 * @brief Returns the current number of free bytes in the kernel heap.
 *
 * @return Free heap bytes.
 *
 * @thread_safety ISR-safe.
 */
static inline uint32_t Kalango_GetHeapFreeBytes(void) {
    return GetKernelFreeBytesOnHeap();
}

/**
 * @brief Puts the current task to sleep for a number of ticks.
 *
 * @param ticks Number of ticks to sleep. Must be > 0.
 * @return KernelResult status.
 *
 * @retval kSuccess Slept and resumed normally.
 * @retval kErrorInvalidParam ticks == 0.
 * @retval kStatusSchedLocked Scheduler is locked, so no context switch occurred.
 * @retval kErrorInvalidKernelState Scheduling produced invalid state.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_Sleep(uint32_t ticks) {
    return Sleep(ticks);
}

/** @} */ /* kalango_core */

/* -------------------------------------------------------------------------- */
/* Tasks                                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @defgroup kalango_task Tasks
 * @ingroup kalango_api
 * @brief Task creation and control.
 * @{
 */

/**
 * @brief Creates a new task and makes it ready to run.
 *
 * @param settings Pointer to a `TaskSettings` structure (stack, entry, priority, etc.).
 * @return Created TaskId on success; NULL on failure.
 *
 * @warning This API returns NULL on failure (pointer-typed ID).
 * @thread_safety Not ISR-safe.
 */
static inline TaskId Kalango_TaskCreate(TaskSettings *settings) {
    return TaskCreate(settings);
}

/**
 * @brief Suspends a task.
 *
 * @param task_id Target task ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Task suspended (or request accepted in ISR-safe region).
 * @retval kErrorInvalidParam task_id is NULL.
 * @retval kErrorTaskAlreadySuspended Task already suspended.
 * @retval kErrorInvalidKernelState Called from ISR without proper ISR-safe region.
 *
 * @note ISR usage: allowed only if ISR is wrapped by Kalango_IrqEnter/Leave.
 * @thread_safety Conditionally ISR-safe (requires ISR-safe region).
 */
static inline KernelResult Kalango_TaskSuspend(TaskId task_id) {
    return TaskSuspend(task_id);
}

/**
 * @brief Resumes a previously suspended task.
 *
 * @param task_id Target task ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Task resumed.
 * @retval kErrorInvalidParam task_id is NULL.
 * @retval kErrorTaskAlreadyResumed Task already resumed / not suspended.
 * @retval kErrorInvalidKernelState Called from ISR without proper ISR-safe region.
 *
 * @note ISR usage: allowed only if ISR is wrapped by Kalango_IrqEnter/Leave.
 * @thread_safety Conditionally ISR-safe (requires ISR-safe region).
 */
static inline KernelResult Kalango_TaskResume(TaskId task_id) {
    return TaskResume(task_id);
}

/**
 * @brief Requests deletion of a task.
 *
 * @param task_id Target task ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Deletion request accepted and scheduling resolved.
 * @retval kErrorInvalidParam task_id is NULL.
 * @retval kStatusSchedLocked Scheduler is locked, so no context switch occurred.
 * @retval kErrorInvalidKernelState Scheduling produced invalid state.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_TaskDelete(TaskId task_id) {
    return TaskDelete(task_id);
}

/**
 * @brief Sets the priority of a task.
 *
 * @param task_id Target task ID.
 * @param new_priority New priority value.
 * @return Previous priority on success; special values on error (see warnings).
 *
 * @warning This API returns a uint32_t but may encode errors:
 * - Returns 0xFFFFFFFF on invalid params (assert-style).
 * - If called from ISR without ISR-safe region, it returns `kErrorInvalidKernelState` (enum value) cast to uint32_t.
 *
 * @note ISR usage: allowed only if ISR is wrapped by Kalango_IrqEnter/Leave.
 * @thread_safety Conditionally ISR-safe (requires ISR-safe region).
 */
static inline uint32_t Kalango_TaskSetPriority(TaskId task_id, uint32_t new_priority) {
    return TaskSetPriority(task_id, new_priority);
}

/**
 * @brief Gets the current priority of a task.
 *
 * @param task_id Target task ID.
 * @return Current priority; may return an error code cast to uint32_t on invalid param.
 *
 * @warning If task_id is NULL, internally this hits ASSERT_PARAM and returns `kErrorInvalidParam` cast to uint32_t.
 * @thread_safety ISR-safe (read-only), but beware NULL input.
 */
static inline uint32_t Kalango_TaskGetPriority(TaskId task_id) {
    return TaskGetPriority(task_id);
}

/**
 * @brief Forces the current task to yield the CPU.
 *
 * @return KernelResult status.
 *
 * @retval kSuccess Yield performed.
 * @retval kErrorInsideIsr Called from ISR.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_TaskYield(void) {
    return TaskYield();
}

/** @} */ /* kalango_task */

/* -------------------------------------------------------------------------- */
/* Semaphores                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @defgroup kalango_semaphore Semaphores
 * @ingroup kalango_api
 * @brief Counting semaphores.
 * @{
 */

/**
 * @brief Creates a counting semaphore.
 *
 * @param initial Initial count.
 * @param limit Maximum count (cap). Must be > 0.
 * @return Created semaphore ID; NULL on failure.
 *
 * @thread_safety Not ISR-safe (alloc + scheduling).
 */
static inline SemaphoreId Kalango_SemaphoreCreate(uint32_t initial, uint32_t limit) {
    return SemaphoreCreate(initial, limit);
}

/**
 * @brief Takes (decrements) a semaphore; blocks if not available.
 *
 * @param semaphore Semaphore ID.
 * @param timeout Timeout in ticks.
 * @return KernelResult status.
 *
 * @retval kSuccess Acquired semaphore.
 * @retval kErrorInvalidParam semaphore is NULL.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kErrorTimeout Timed out while waiting.
 * @retval kStatusSemaphoreUnavailable Immediate failure when timeout == KERNEL_NO_WAIT.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_SemaphoreTake(SemaphoreId semaphore, uint32_t timeout) {
    return SemaphoreTake(semaphore, timeout);
}

/**
 * @brief Gives (increments) a semaphore by `count`.
 *
 * @param semaphore Semaphore ID.
 * @param count Number of increments. Must be > 0.
 * @return KernelResult status.
 *
 * @retval kSuccess Gave semaphore (and possibly woke pending tasks).
 * @retval kErrorInvalidParam semaphore is NULL or count == 0.
 * @retval kErrorInvalidKernelState Called from ISR without proper ISR-safe region.
 * @retval kStatusSchedLocked Scheduler locked (when called from thread context via CheckReschedule path).
 * @retval kErrorInvalidKernelState Scheduling produced invalid state (CheckReschedule path).
 *
 * @note ISR usage: allowed only if ISR is wrapped by Kalango_IrqEnter/Leave.
 * @thread_safety Conditionally ISR-safe (requires ISR-safe region).
 */
static inline KernelResult Kalango_SemaphoreGive(SemaphoreId semaphore, uint32_t count) {
    return SemaphoreGive(semaphore, count);
}

/**
 * @brief Deletes a semaphore.
 *
 * @param semaphore Semaphore ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Deleted.
 * @retval kErrorInvalidParam semaphore is NULL.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kStatusSchedLocked Scheduler locked (may prevent immediate switch).
 * @retval kErrorInvalidKernelState Scheduling produced invalid state.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_SemaphoreDelete(SemaphoreId semaphore) {
    return SemaphoreDelete(semaphore);
}

/** @} */ /* kalango_semaphore */

/* -------------------------------------------------------------------------- */
/* Mutex                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @defgroup kalango_mutex Mutexes
 * @ingroup kalango_api
 * @brief Mutual exclusion locks (supports priority ceiling behavior in current implementation).
 * @{
 */

/**
 * @brief Creates a mutex.
 *
 * @return Created mutex ID; NULL on failure.
 *
 * @thread_safety Not ISR-safe (alloc + scheduling).
 */
static inline MutexId Kalango_MutexCreate(void) {
    return MutexCreate();
}

/**
 * @brief Tries to lock a mutex without blocking.
 *
 * @param mutex Mutex ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Locked (and scheduling resolved).
 * @retval kErrorInvalidParam mutex is NULL.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kStatusMutexAlreadyTaken Mutex already locked.
 * @retval kStatusSchedLocked Scheduler locked.
 * @retval kErrorInvalidKernelState Scheduling produced invalid state.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_MutexTryLock(MutexId mutex) {
    return MutexTryLock(mutex);
}

/**
 * @brief Locks a mutex, optionally blocking up to `timeout`.
 *
 * @param mutex Mutex ID.
 * @param timeout Timeout in ticks.
 * @return KernelResult status.
 *
 * @retval kSuccess Locked.
 * @retval kErrorInvalidParam mutex is NULL.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kErrorTimeout Timed out while waiting.
 * @retval kStatusMutexAlreadyTaken Immediate failure on re-lock attempt in this implementation.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_MutexLock(MutexId mutex, uint32_t timeout) {
    return MutexLock(mutex, timeout);
}

/**
 * @brief Unlocks a mutex.
 *
 * @param mutex Mutex ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Unlocked (and scheduling resolved).
 * @retval kErrorInvalidParam mutex is NULL.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kErrorInvalidMutexOwner Current task is not the owner.
 * @retval kStatusSchedLocked Scheduler locked.
 * @retval kErrorInvalidKernelState Scheduling produced invalid state.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_MutexUnlock(MutexId mutex) {
    return MutexUnlock(mutex);
}

/**
 * @brief Deletes a mutex.
 *
 * @param mutex Mutex ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Deleted.
 * @retval kErrorInvalidParam mutex is NULL.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kStatusSchedLocked Scheduler locked.
 * @retval kErrorInvalidKernelState Scheduling produced invalid state.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_MutexDelete(MutexId mutex) {
    return MutexDelete(mutex);
}

/** @} */ /* kalango_mutex */

/* -------------------------------------------------------------------------- */
/* Queue                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @defgroup kalango_queue Queues
 * @ingroup kalango_api
 * @brief Fixed-slot message queues for inter-task communication.
 * @{
 */

/**
 * @brief Creates a queue with a fixed number of slots and fixed slot size.
 *
 * @param noof_slots Number of slots. Must be > 0.
 * @param slot_size Size (bytes) of each slot. Must be > 0.
 * @return Created queue ID; NULL on failure.
 *
 * @thread_safety Not ISR-safe (alloc + scheduling).
 */
static inline QueueId Kalango_QueueCreate(uint32_t noof_slots, uint32_t slot_size) {
    return QueueCreate(noof_slots, slot_size);
}

/**
 * @brief Inserts data into the queue; may block if the queue is full.
 *
 * @param queue Queue ID.
 * @param data Pointer to input buffer.
 * @param data_size Input size (bytes).
 * @param timeout Timeout in ticks.
 * @return KernelResult status.
 *
 * @retval kSuccess Inserted.
 * @retval kErrorInvalidParam queue/data invalid or size invalid.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kErrorBufferFull Queue full with timeout == KERNEL_NO_WAIT.
 * @retval kErrorTimeout Timed out while waiting.
 * @retval kErrorInvalidKernelState Called from ISR without proper ISR-safe region (internal check path).
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_QueueInsert(QueueId queue, void *data, uint32_t data_size, uint32_t timeout) {
    return QueueInsert(queue, data, data_size, timeout);
}

/**
 * @brief Peeks the next queue element without removing it; may block if empty.
 *
 * @param queue Queue ID.
 * @param data Output buffer.
 * @param data_size [in,out] On input: buffer capacity; on output: copied bytes.
 * @param timeout Timeout in ticks.
 * @return KernelResult status.
 *
 * @retval kSuccess Peeked.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kErrorBufferEmpty Queue empty with timeout == KERNEL_NO_WAIT.
 * @retval kErrorTimeout Timed out while waiting.
 * @retval kErrorInvalidKernelState Called from ISR without proper ISR-safe region (internal check path).
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_QueuePeek(QueueId queue, void *data, uint32_t *data_size, uint32_t timeout) {
    return QueuePeek(queue, data, data_size, timeout);
}

/**
 * @brief Removes the next queue element; may block if empty.
 *
 * @param queue Queue ID.
 * @param data Output buffer.
 * @param data_size [in,out] On input: buffer capacity; on output: copied bytes.
 * @param timeout Timeout in ticks.
 * @return KernelResult status.
 *
 * @retval kSuccess Removed.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kErrorBufferEmpty Queue empty with timeout == KERNEL_NO_WAIT.
 * @retval kErrorTimeout Timed out while waiting.
 * @retval kErrorInvalidKernelState Called from ISR without proper ISR-safe region (internal check path).
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_QueueRemove(QueueId queue, void *data, uint32_t *data_size, uint32_t timeout) {
    return QueueRemove(queue, data, data_size, timeout);
}

/**
 * @brief Deletes a queue.
 *
 * @param queue Queue ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Deleted.
 * @retval kErrorInvalidParam queue is NULL.
 * @retval kErrorInsideIsr Called from ISR.
 * @retval kStatusSchedLocked Scheduler locked.
 * @retval kErrorInvalidKernelState Scheduling produced invalid state.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_QueueDelete(QueueId queue) {
    return QueueDelete(queue);
}

/** @} */ /* kalango_queue */

/* -------------------------------------------------------------------------- */
/* Timers                                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @defgroup kalango_timer Timers
 * @ingroup kalango_api
 * @brief Software timers (callbacks) driven by system ticks.
 * @{
 */

/**
 * @brief Creates a timer.
 *
 * @param cb Callback executed on expiry.
 * @param expiry_time Time (ticks) to first fire.
 * @param period_time Period (ticks) for periodic timers.
 * @param user_data Opaque pointer passed to callback.
 * @return Created timer ID; NULL on failure.
 *
 * @thread_safety Not ISR-safe (alloc + scheduling).
 */
static inline TimerId Kalango_TimerCreate(TimerCallback cb, uint32_t expiry_time, uint32_t period_time, void *user_data) {
    return TimerCreate(cb, expiry_time, period_time, user_data);
}

/**
 * @brief Starts (arms) a timer.
 *
 * @param timer Timer ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Started.
 * @retval kErrorInvalidParam timer is NULL or internal timeout params invalid.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_TimerStart(TimerId timer) {
    return TimerStart(timer);
}

/**
 * @brief Stops (disarms) a timer.
 *
 * @param timer Timer ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Stopped.
 * @retval kErrorInvalidParam timer is NULL.
 * @retval kErrorTimerIsNotRunning Timer not running.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_TimerStop(TimerId timer) {
    return TimerStop(timer);
}

/**
 * @brief Updates timer expiry and period values.
 *
 * @param timer Timer ID.
 * @param expiry_time Time (ticks) to next fire.
 * @param period_time Period (ticks) for subsequent fires.
 * @return KernelResult status.
 *
 * @retval kSuccess Updated.
 * @retval kErrorInvalidParam timer is NULL or invalid values.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_TimerSetValues(TimerId timer, uint32_t expiry_time, uint32_t period_time) {
    return TimerSetValues(timer, expiry_time, period_time);
}

/**
 * @brief Deletes a timer.
 *
 * @param timer Timer ID.
 * @return KernelResult status.
 *
 * @retval kSuccess Deleted.
 * @retval kErrorInvalidParam timer is NULL.
 *
 * @thread_safety Not ISR-safe.
 */
static inline KernelResult Kalango_TimerDelete(TimerId timer) {
    return TimerDelete(timer);
}

/** @} */ /* kalango_timer */

/* -------------------------------------------------------------------------- */
/* Critical Section                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @defgroup kalango_critical Critical Section
 * @ingroup kalango_api
 * @brief Critical section helpers (port-dependent).
 * @{
 */

/**
 * @brief Enters a critical section.
 *
 * @return KernelResult status (typically kSuccess).
 * @thread_safety ISR-safe (implementation/port dependent).
 */
static inline KernelResult Kalango_CriticalEnter(void) {
    return ArchCriticalSectionEnter();
}

/**
 * @brief Leaves a critical section.
 *
 * @return KernelResult status (typically kSuccess).
 * @thread_safety ISR-safe (implementation/port dependent).
 */
static inline KernelResult Kalango_CriticalExit(void) {
    return ArchCriticalSectionExit();
}

/** @} */ /* kalango_critical */

/* -------------------------------------------------------------------------- */
/* ISR Safe Region                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @defgroup kalango_isr ISR Safe Region
 * @ingroup kalango_api
 * @brief Delimits an RTOS-aware ISR region (port-dependent).
 * @{
 */

/**
 * @brief Marks entry into an ISR region (notifies the kernel).
 *
 * @return KernelResult status (typically kSuccess).
 *
 * @note Place as the first instruction in the ISR.
 * @thread_safety ISR-only.
 */
static inline KernelResult Kalango_IrqEnter(void) {
    return ArchIsrEnter();
}

/**
 * @brief Marks exit from an ISR region (notifies the kernel).
 *
 * @return KernelResult status (typically kSuccess).
 *
 * @note Place as the last instruction in the ISR.
 * @thread_safety ISR-only.
 */
static inline KernelResult Kalango_IrqLeave(void) {
    return ArchIsrLeave();
}

/** @} */ /* kalango_isr */

/** @} */ /* kalango_api */