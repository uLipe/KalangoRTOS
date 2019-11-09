#pragma once

#include <KalangoRTOS/kernel_types.h>
#include <KalangoRTOS/core.h>
#include <KalangoRTOS/sched.h>
#include <KalangoRTOS/clock.h>
#include <KalangoRTOS/object_pool.h>
#include <KalangoRTOS/task.h>

MutexId MutexCreate();
KernelResult MutexTryLock(MutexId mutex);
KernelResult MutexLock(MutexId mutex, uint32_t timeout);
KernelResult MutexUnlock(MutexId mutex);
KernelResult MutexDelete(MutexId mutex);
