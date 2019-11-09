#pragma once 

#include <kernel_types.h>
#include <irq.h>
#include <clock.h>
#include <core.h>


KernelResult ArchInitializeSpecifics();
KernelResult ArchStartKernel();
KernelResult ArchSwitchFromInterrupt();
KernelResult ArchSwitchFromTask();
KernelResult ArchNewTask(TaskControBlock *task, uint8_t *stack_base, uint32_t stack_size);
uint8_t ArchCountLeadZeros(uint32_t word);
