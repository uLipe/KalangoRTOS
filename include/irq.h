#pragma once 

#include <kernel_types.h>

KernelResult IrqEnable();
KernelResult IrqDisable();
KernelResult IrqInstallHandler(uint32_t handler, int32_t irq_number, uint32_t priority);
KernelResult IrqEnableHandler(int32_t irq_number);
KernelResult IrqDisableHandler(int32_t irq_number);
KernelResult IrqEnter();
KernelResult IrqLeave();
bool IsInsideIsr();

