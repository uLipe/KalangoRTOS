#pragma once

#include <KalangoRTOS/kernel_types.h>
#include <KalangoRTOS/core.h>
#include <KalangoRTOS/sched.h>
#include <KalangoRTOS/clock.h>
#include <KalangoRTOS/object_pool.h>

SemaphoreId SemaphoreCreate(uint32_t initial, uint32_t limit);
KernelResult SemaphoreTake(SemaphoreId semaphore, uint32_t timeout);
KernelResult SemaphoreGive(SemaphoreId semaphore, uint32_t count);
KernelResult SemaphoreDelete (SemaphoreId semaphore);