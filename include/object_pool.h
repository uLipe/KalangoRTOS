#pragma once 

#include <arch.h>
#include <kernel_types.h>
#include <list.h>

uint32_t GetKernelFreeBytesOnHeap();

KernelResult InitializeObjectPools();

uint8_t *AllocateRawBuffer(uint32_t size);
KernelResult FreeRawBuffer(uint8_t *self);

TaskControBlock *AllocateTaskObject();
KernelResult FreeTaskObject(TaskControBlock *self);

Semaphore *AllocateSemaphoreObject();
KernelResult FreeSemaphoreObject(Semaphore *self);

Mutex *AllocateMutexObject();
KernelResult FreeMutexObject(Mutex *self);

Timer *AllocateTimerObject();
KernelResult FreeTimerObject(Timer *self);

Queue *AllocateQueueObject();
KernelResult FreeQueueObject(Queue *self);
