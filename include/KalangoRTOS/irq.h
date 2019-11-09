#pragma once

#include <KalangoRTOS/kernel_types.h>

#define IRQ_INDEX_INITIAL_SP    0U
#define IRQ_INDEX_RESET         1U

void IrqSwitchComplete(void);

void IrqTableInit(void);
void IrqBind(uint32_t irq_index, void (*handler)(void));
void IrqDispatch(uint32_t irq_index);

KernelResult IrqRequest(uint32_t irq_index, void (*handler)(void), uint32_t priority);
KernelResult IrqDetach(uint32_t irq_index);
KernelResult IrqEnableHandler(uint32_t irq_index);
KernelResult IrqDisableHandler(uint32_t irq_index);

uint32_t IrqGetNestLevel(void);
bool IrqInHandler(void);

void IrqSwitchComplete(void);
