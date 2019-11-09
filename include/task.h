#pragma once 

#include <kernel_types.h>
#include <core.h>
#include <sched.h>
#include <clock.h>
#include <irq.h>
#include <object_pool.h>

TaskId TaskCreate(TaskSettings *settings);
KernelResult TaskSuspend(TaskId task_id);
KernelResult TaskResume(TaskId task_id);
KernelResult TaskDelete(TaskId task_id);
uint32_t TaskSetPriority(TaskId task_id, uint32_t new_priority);
uint32_t TaskGetPriority(TaskId task_id);
KernelResult TaskYield();
