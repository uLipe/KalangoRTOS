#pragma once

#include <KalangoRTOS/kernel_types.h>
#include <KalangoRTOS/core.h>
#include <KalangoRTOS/sched.h>
#include <KalangoRTOS/clock.h>
#include <KalangoRTOS/object_pool.h>

TaskId TaskCreate(TaskSettings *settings);
KernelResult TaskSuspend(TaskId task_id);
KernelResult TaskResume(TaskId task_id);
KernelResult TaskDelete(TaskId task_id);
uint32_t TaskSetPriority(TaskId task_id, uint32_t new_priority);
uint32_t TaskGetPriority(TaskId task_id);
KernelResult TaskYield();
