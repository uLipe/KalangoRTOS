#include <KalangoRTOS/irq.h>
#include <KalangoRTOS/arch.h>
#include <KalangoRTOS/core.h>
#include <KalangoRTOS/kalango_config_internal.h>

static void (*isr_dispatch_table[CONFIG_ISR_TABLE_SIZE])(void);
static uint32_t irq_nest_level;

static void IrqSpurious(void)
{
    for (;;) {
    }
}

void IrqTableInit(void)
{
    for (uint32_t i = 0; i < CONFIG_ISR_TABLE_SIZE; i++) {
        isr_dispatch_table[i] = IrqSpurious;
    }
}

void IrqBind(uint32_t irq_index, void (*handler)(void))
{
    if (irq_index >= CONFIG_ISR_TABLE_SIZE || handler == NULL) {
        return;
    }

    isr_dispatch_table[irq_index] = handler;
}

void IrqDispatch(uint32_t irq_index)
{
    void (*handler)(void);

    if (irq_index >= CONFIG_ISR_TABLE_SIZE) {
        irq_index = 0;
    }

    handler = isr_dispatch_table[irq_index];

    irq_nest_level++;
    handler();
    irq_nest_level--;
}

KernelResult IrqRequest(uint32_t irq_index, void (*handler)(void), uint32_t priority)
{
    ASSERT_PARAM(handler);

    if (irq_index < IRQ_INDEX_RESET + 1U || irq_index >= CONFIG_ISR_TABLE_SIZE) {
        return kErrorInvalidParam;
    }

    if (ArchIrqDenyUserInstall(irq_index)) {
        return kErrorInvalidParam;
    }

    if (priority >= CONFIG_IRQ_PRIORITY_LEVELS) {
        return kErrorInvalidParam;
    }

    ArchCriticalSectionEnter();
    isr_dispatch_table[irq_index] = handler;
    ArchIrqSetPriority(irq_index, priority);
    ArchCriticalSectionExit();

    return kSuccess;
}

KernelResult IrqDetach(uint32_t irq_index)
{
    if (irq_index < IRQ_INDEX_RESET + 1U || irq_index >= CONFIG_ISR_TABLE_SIZE) {
        return kErrorInvalidParam;
    }

    if (ArchIrqDenyUserInstall(irq_index)) {
        return kErrorInvalidParam;
    }

    ArchCriticalSectionEnter();
    isr_dispatch_table[irq_index] = IrqSpurious;
    ArchCriticalSectionExit();

    return kSuccess;
}

KernelResult IrqEnableHandler(uint32_t irq_index)
{
    if (irq_index < IRQ_INDEX_RESET + 1U || irq_index >= CONFIG_ISR_TABLE_SIZE) {
        return kErrorInvalidParam;
    }

    return ArchIrqEnable(irq_index);
}

KernelResult IrqDisableHandler(uint32_t irq_index)
{
    if (irq_index < IRQ_INDEX_RESET + 1U || irq_index >= CONFIG_ISR_TABLE_SIZE) {
        return kErrorInvalidParam;
    }

    return ArchIrqDisable(irq_index);
}

uint32_t IrqGetNestLevel(void)
{
    return irq_nest_level;
}

bool IrqInHandler(void)
{
    return irq_nest_level != 0U;
}

void IrqSwitchComplete(void)
{
    irq_nest_level = 0U;
}
