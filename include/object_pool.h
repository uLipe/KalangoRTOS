#pragma once 

#include <kernel_types.h>
#include <list.h>
#include <irq.h>

KernelResult InitializeObjectPools();

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
