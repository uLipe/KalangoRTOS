#pragma once 

#include <kernel_types.h>
#include <core.h>
#include <sched.h>
#include <clock.h>
#include <object_pool.h>
#include <task.h>

MutexId MutexCreate();
KernelResult MutexTryLock(MutexId mutex);
KernelResult MutexLock(MutexId mutex, uint32_t timeout);
KernelResult MutexUnlock(MutexId mutex);
KernelResult MutexDelete(MutexId mutex);
