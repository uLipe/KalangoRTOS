#pragma once 

#include <kernel_types.h>
#include <clock.h>
#include <core.h>


KernelResult ArchInitializeSpecifics();
KernelResult ArchStartKernel();
KernelResult ArchNewTask(TaskControBlock *task, uint8_t *stack_base, uint32_t stack_size);
KernelResult ArchCriticalSectionEnter();
KernelResult ArchCriticalSectionExit();
KernelResult ArchYield();
KernelResult ArchIsrEnter();
KernelResult ArchIsrLeave();
bool ArchInIsr();
uint32_t ArchGetIsrNesting();    
uint8_t ArchCountLeadZeros(uint32_t word);
