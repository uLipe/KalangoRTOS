#pragma once

#include <KalangoRTOS/kernel_types.h>
#include <KalangoRTOS/sched.h>
#include <KalangoRTOS/arch.h>
#include <KalangoRTOS/platform.h>

KernelResult CoreInit();
KernelResult CoreStart();
KernelResult CoreMakeTaskReady(TaskControBlock * task);
KernelResult CheckReschedule();
KernelResult CoreManageRoundRobin();
KernelResult CoreInitializeTaskList(TaskPriorityList *list);
TaskControBlock * CoreGetCurrentTask();
TaskControBlock * CoreTaskSwitch();
KernelResult CoreSchedulingSuspend();
KernelResult CoreSchedulingResume();

KernelResult CoreInitWaitQueue(WaitQueue *wq);
KernelResult CoreUnpendNextTask(WaitQueue *wq);
KernelResult CoreMakeAllTasksReady(WaitQueue *wq);
TaskControBlock *CorePeekWaitQueue(WaitQueue *wq);
KernelResult CoreMakeTaskPending(TaskControBlock * task, uint32_t reason, WaitQueue *wq);

bool IsCoreRunning();