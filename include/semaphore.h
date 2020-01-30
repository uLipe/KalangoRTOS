#pragma once 

#include <kernel_types.h>
#include <core.h>
#include <sched.h>
#include <clock.h>
#include <object_pool.h>

SemaphoreId SemaphoreCreate(uint32_t initial, uint32_t limit);
KernelResult SemaphoreTake(SemaphoreId semaphore, uint32_t timeout);
KernelResult SemaphoreGive(SemaphoreId semaphore, uint32_t count);
KernelResult SemaphoreDelete (SemaphoreId semaphore);