#pragma once

#include <kernel_types.h>
#include <sched.h>
#include <arch.h>
#include <irq.h>
#include <platform.h>

KernelResult CoreInit();
KernelResult CoreStart();
KernelResult CoreMakeTaskPending(TaskControBlock * task, uint32_t reason, TaskPriorityList *kobject_pending_list);
KernelResult CoreUnpendNextTask(TaskPriorityList *kobject_pending_list);
KernelResult CoreMakeTaskReady(TaskControBlock * task);
KernelResult CoreMakeAllTasksReady(TaskPriorityList *tasks);
KernelResult CheckReschedule();
KernelResult CoreManageRoundRobin();
KernelResult CoreInitializeTaskList(TaskPriorityList *list);
TaskControBlock * CoreGetCurrentTask();
TaskControBlock * CoreTaskSwitch();
KernelResult CoreSchedulingSuspend();
KernelResult CoreSchedulingResume();
bool IsCoreRunning();