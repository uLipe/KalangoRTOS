#pragma once 

#include <kernel_types.h>
#include <arch.h>
#include <irq.h>

bool IsSchedulerLocked(TaskPriorityList *taskset);
KernelResult SchedulerLock(TaskPriorityList *taskset);
KernelResult SchedulerUnlock(TaskPriorityList *taskset);
TaskControBlock * ScheduleTaskSet(TaskPriorityList *taskset);
void SchedulerInitTaskPriorityList(TaskPriorityList *list);
bool NothingToSched(TaskPriorityList *list);
KernelResult SchedulerSetPriority(TaskPriorityList *list, uint32_t priority);
KernelResult SchedulerResetPriority(TaskPriorityList *list, uint32_t priority);