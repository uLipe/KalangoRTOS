#pragma once

#include <kalango_config.h>

#ifndef __ASSEMBLER__
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <macros.h>
#include <list.h>
#include "kernel_objects.h"

#if CONFIG_PRIORITY_LEVELS > 32
#error "Maximum priority level allowed is 32"
#endif

#define TASK_STATE_READY                0
#define TASK_STATE_PEND_SEMAPHORE       1
#define TASK_STATE_SUPENDED             2
#define TASK_STATE_PEND_QUEUE           4
#define TASK_STATE_PEND_TIMEOUT         8
#define TASK_STATE_PEND_MUTEX           16
#define TASK_STATE_PEND_ALL_SIGNALS     32
#define TASK_STATE_PEND_ANY_SIGNAL      64
#define TASK_STATE_TERMINATED           128
#define TASK_STATE_WOKEN_BY_TIMEOUT     256

#define KERNEL_WAIT_FOREVER             -1
#define KERNEL_NO_WAIT                   0

typedef enum {
    kSuccess = 0,
    kErrorInvalidParam,
    kErrorBufferFull,
    kErrorBufferEmpty,
    kErrorTimeout,
    kErrorDeviceBusy,
    kErrorInsideIsr,
    kErrorNotEnoughKernelMemory,
    kErrorTimerIsNotRunning,
    kStatusNoSwitchPending,
    kStatusSwitchIsPending,
    kStatusSchedLocked,
    kStatusSchedUnlocked,
    kErrorInvalidKernelState,
    kStatusMutexAlreadyTaken,
    kStatusSemaphoreUnavailable,
    kErrorTaskAlreadySuspended,
    kErrorTaskAlreadyResumed,
    kErrorInvalidMutexOwner,
    kErrorNothingToSchedule,
}KernelResult;

typedef void (*TimerCallback) (void *user_data);

typedef TaskControBlock * TaskId;
typedef Timer* TimerId;
typedef Queue* QueueId;
typedef Mutex* MutexId;
typedef Semaphore * SemaphoreId;

typedef void (*TaskFunction) (void *arg);

typedef struct {
    uint32_t priority;
    uint8_t  *stack_area;
    uint32_t stack_size;
    TaskFunction function;
    void *arg;
} TaskSettings;

#ifndef CONFIG_REMOVE_CHECKINGS
#define ASSERT_KERNEL(x, ...)       \
        if(!(x)) {                  \
            return __VA_ARGS__;     \
        }                           
#else
    #define ASSERT_KERNEL(x, ...)
#endif

#define ASSERT_PARAM(x)     ASSERT_KERNEL(x, kErrorInvalidParam)
#endif