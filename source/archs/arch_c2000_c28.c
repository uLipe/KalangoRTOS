#include <KalangoRTOS/arch.h>
#include <KalangoRTOS/irq.h>
#include <KalangoRTOS/macros.h>

#if CONFIG_ARCH_C2000_C28 > 0

#include "arch_c2000_c28_defs.h"

static uint32_t irq_lock_level  = 0U;
static uint32_t irq_nest_level  = 0U;

/*
 * Current active PIE group being dispatched.
 * Set by the group ISR wrappers before calling the common handler.
 */
static uint32_t c28_active_irq_index = 0U;

/* ----------------------------------------------------------------------------
 * Forward declarations for asm helpers
 * --------------------------------------------------------------------------*/
extern void ArchTickIsr(void);
extern void ArchSwitchIsr(void);
extern void ArchSwIrqIsr(void);

/* Forward declarations for PIE group wrappers (defined in asm file) */
extern void ArchPieGroup1Isr(void);
extern void ArchPieGroup2Isr(void);
extern void ArchPieGroup3Isr(void);
extern void ArchPieGroup4Isr(void);
extern void ArchPieGroup5Isr(void);
extern void ArchPieGroup6Isr(void);
extern void ArchPieGroup7Isr(void);
extern void ArchPieGroup8Isr(void);
extern void ArchPieGroup9Isr(void);
extern void ArchPieGroup10Isr(void);
extern void ArchPieGroup11Isr(void);
extern void ArchPieGroup12Isr(void);

/* ----------------------------------------------------------------------------
 * PIE group ISR common handler — called from each group wrapper after setting
 * c28_active_irq_index to the resolved PIE irq_index.
 * --------------------------------------------------------------------------*/
void ArchPieCommonDispatch(void)
{
    IrqDispatch(c28_active_irq_index);
}

/* ----------------------------------------------------------------------------
 * Called from the PIE group wrapper with (group, which_channel_bit).
 * Resolves the channel from PIE IFR, sets c28_active_irq_index, and triggers
 * ACK so the PIE can re-arm the group.
 * --------------------------------------------------------------------------*/
void ArchPieGroupDispatch(uint32_t group)
{
    uint16_t ifr;
    uint16_t mask;
    uint32_t channel;

    ifr = ARCH_HWREG16(CONFIG_ARCH_PIECTRL_BASE + PIECTRL_O_IFR(group));

    /* Find lowest set channel bit (channels are 1-8, bits 0-7 in PIE IFR) */
    channel = 0U;
    mask = ifr;
    while (mask && !(mask & 1U)) {
        mask >>= 1U;
        channel++;
    }

    c28_active_irq_index = ARCH_C28_PIE_INDEX(group, channel + 1U);

    /* ACK the PIE group so new interrupts can be accepted */
    ARCH_HWREG16(CONFIG_ARCH_PIECTRL_BASE + PIECTRL_O_PIEACK) = (uint16_t)(1U << (group - 1U));

    IrqDispatch(c28_active_irq_index);
}

/* ----------------------------------------------------------------------------
 * ArchInitializeSpecifics — configure CPU Timer 2, RTOSINT, DLOGINT, PIE.
 * --------------------------------------------------------------------------*/
KernelResult ArchInitializeSpecifics(void)
{
    uint16_t tcr;

    /* Stop CPU Timer 2 */
    tcr = ARCH_HWREG16(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_TCR);
    tcr |= CPUTIMER_TCR_TSS;
    ARCH_HWREG16(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_TCR) = tcr;

    /* Set period: (SysClock / TicksPerSec) - 1, split into 32-bit PRD */
    uint32_t period = (CONFIG_PLATFORM_SYS_CLOCK_HZ / CONFIG_TICKS_PER_SEC) - 1U;
    ARCH_HWREG32(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_PRD) = period;

    /* Zero prescaler */
    ARCH_HWREG16(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_TPR)  = 0U;
    ARCH_HWREG16(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_TPRH) = 0U;

    /* Reload counter and clear flag */
    tcr = ARCH_HWREG16(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_TCR);
    tcr &= ~CPUTIMER_TCR_TIF;
    tcr |= (CPUTIMER_TCR_TRB | CPUTIMER_TCR_TIE);
    ARCH_HWREG16(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_TCR) = tcr;

    /* Start timer */
    tcr = ARCH_HWREG16(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_TCR);
    tcr &= ~CPUTIMER_TCR_TSS;
    ARCH_HWREG16(CONFIG_ARCH_CPUTIMER2_BASE + CPUTIMER_O_TCR) = tcr;

    /*
     * Enable PIE: set ENPIE bit so the CPU fetches interrupt vectors from
     * the PIE vector table. The PIE vector table must be populated before
     * global interrupts are enabled.
     */
    ARCH_HWREG16(CONFIG_ARCH_PIECTRL_BASE + PIECTRL_O_PIECTRL) |= 0x0001U;

    /* ACK all PIE groups to clear any spurious pending flags */
    ARCH_HWREG16(CONFIG_ARCH_PIECTRL_BASE + PIECTRL_O_PIEACK) = PIE_ACK_ALL;

    /* IER is configured in ArchStartKernel via asm (EINT/IER manipulation) */

    return kSuccess;
}

/* ----------------------------------------------------------------------------
 * ArchNewTask — initialise the stack frame of a new task.
 *
 * Stack layout (top = lower address = first popped):
 *   [manually saved, top → bottom]
 *     RPC
 *     XT  (32-bit = 2 words)
 *     XAR6 XAR5 XAR4 XAR3 XAR2 XAR1 XAR0  (each 32-bit = 2 words)
 *   [FPU, if CONFIG_HAS_FLOAT, top → bottom]
 *     RB STF R7H R6H R5H R4H R3H R2H R1H R0H
 *   [auto-saved by hardware (IRET restores these), top → bottom]
 *     IER DBGSTAT ST1 DP AR1H AR0H PH PL AH AL ST0 T
 *     PC (22-bit in 2 words)
 *
 * Note: C28x stack is 16-bit word addressed. SP points to the next free word.
 *       PUSH decrements SP by 1 (or 2 for 32-bit) before storing.
 *       First word pushed is at the highest address.
 * --------------------------------------------------------------------------*/
KernelResult ArchNewTask(TaskControBlock *task, uint8_t *stack_base, uint32_t stack_size)
{
    ASSERT_PARAM(task);
    ASSERT_PARAM(stack_base);
    ASSERT_PARAM(stack_size);

    ArchCriticalSectionEnter();

    /* Align stack base to 32-bit boundary */
    uint16_t *sp = (uint16_t *)ALIGN((uint32_t)stack_base, 4);
    uint32_t words = stack_size / sizeof(uint16_t);
    uint32_t pc;
    uint32_t arg;
    int32_t i;

    /* Start from top of the stack region */
    sp = sp + words;

    /* --- Hardware auto-saved frame (pushed from bottom, restored by IRET) --- */
    /* IRET pops in reverse order of hardware push sequence */

    /* PC (22-bit return address) = task entry point */
    pc = ((uint32_t)task->entry_point) & 0x003FFFFFU;
    *--sp = (uint16_t)(pc & 0xFFFFU);          /* PC low */
    *--sp = (uint16_t)((pc >> 16U) & 0x003FU); /* PC high (6 bits) */

    /* IER: enable RTOSINT and DLOGINT */
    *--sp = (uint16_t)ARCH_C28_INIT_IER_VALUE;

    *--sp = 0x0000U;  /* DBGSTAT */

    /* ST1: C28AMODE=1, VMAP=1, SPA=1, INTM=0 (interrupts enabled) */
    *--sp = ARCH_C28_INIT_ST1;

    *--sp = 0x0000U;  /* DP */
    *--sp = 0x0000U;  /* AR1H */
    *--sp = 0x0000U;  /* AR0H */
    *--sp = 0x0000U;  /* PH */
    *--sp = 0x0000U;  /* PL */
    *--sp = 0x0000U;  /* AH */
    *--sp = 0x0000U;  /* AL */

    *--sp = ARCH_C28_INIT_ST0;  /* ST0 */
    *--sp = 0x0000U;            /* T (upper half of XT) */

    /* --- Manually saved frame (saved/restored by SAVE/RESTORE_CONTEXT) --- */

#if CONFIG_HAS_FLOAT > 0
    *--sp = 0x0000U; *--sp = 0x0000U;  /* RB */
    *--sp = 0x0000U; *--sp = 0x0000U;  /* STF */
    for (i = 7; i >= 0; i--) {
        *--sp = 0x0000U; *--sp = 0x0000U;
    }
#endif

    *--sp = 0x0000U; *--sp = 0x0000U;  /* RPC */
    *--sp = 0x0000U; *--sp = 0x0000U;  /* XT lower half */

    /* XAR6..XAR0 (XAR4 receives task argument per EABI) */
    for (i = 6; i >= 0; i--) {
        if (i == 4) {
            arg = (uint32_t)task->arg1;
            *--sp = (uint16_t)((arg >> 16U) & 0xFFFFU);
            *--sp = (uint16_t)(arg & 0xFFFFU);
        } else {
            *--sp = 0x0000U; *--sp = 0x0000U;
        }
    }

    task->stackpointer = (uint8_t *)sp;

    ArchCriticalSectionExit();
    return kSuccess;
}

/* ----------------------------------------------------------------------------
 * Critical section
 * --------------------------------------------------------------------------*/
KernelResult ArchCriticalSectionEnter(void)
{
    if (irq_lock_level < 0xFFFFFFFFU) {
        irq_lock_level++;
    }
    if (irq_lock_level == 1U) {
        ARCH_DISABLE_INTERRUPTS();
    }
    return kSuccess;
}

KernelResult ArchCriticalSectionExit(void)
{
    if (irq_lock_level) {
        irq_lock_level--;
    }
    if (!irq_lock_level) {
        ARCH_ENABLE_INTERRUPTS();
    }
    return kSuccess;
}

/* ----------------------------------------------------------------------------
 * ArchYield — trigger RTOSINT to perform a voluntary context switch.
 * --------------------------------------------------------------------------*/
KernelResult ArchYield(void)
{
    if (ArchInIsr()) {
        return kSuccess;
    }
    /*
     * Force RTOSINT by setting IFR bit 13.
     * The CPU will accept it after the current instruction stream drains.
     */
    __asm(" OR IFR, #0x2000");
    __asm(" NOP");
    __asm(" NOP");
    return kSuccess;
}

/* ----------------------------------------------------------------------------
 * ISR nesting tracking
 * --------------------------------------------------------------------------*/
KernelResult ArchIsrEnter(void)
{
    if (irq_nest_level < 0xFFFFFFFFU) {
        irq_nest_level++;
    }
    return kSuccess;
}

KernelResult ArchIsrLeave(void)
{
    if (irq_nest_level) {
        irq_nest_level--;
    }
    if (!irq_nest_level) {
        CheckReschedule();
    }
    return kSuccess;
}

uint32_t ArchGetIsrNesting(void)
{
    return irq_nest_level;
}

/*
 * ArchInIsr — return true if executing inside an interrupt service routine.
 * On C28x, ST1.INTM is set automatically when an interrupt fires.
 * We detect ISR context via the irq_nest_level counter.
 */
bool ArchInIsr(void)
{
    return (irq_nest_level > 0U);
}

/* ----------------------------------------------------------------------------
 * Miscellaneous
 * --------------------------------------------------------------------------*/
uint8_t ArchCountLeadZeros(uint32_t word)
{
    uint8_t n;
    if (word == 0U) {
        return 32U;
    }
    n = 0U;
    if ((word & 0xFFFF0000U) == 0U) { n += 16U; word <<= 16U; }
    if ((word & 0xFF000000U) == 0U) { n +=  8U; word <<=  8U; }
    if ((word & 0xF0000000U) == 0U) { n +=  4U; word <<=  4U; }
    if ((word & 0xC0000000U) == 0U) { n +=  2U; word <<=  2U; }
    if ((word & 0x80000000U) == 0U) { n +=  1U; }
    return n;
}

bool ArchIrqDenyUserInstall(uint32_t irq_index)
{
    return (irq_index == ARCH_C28_TICK_IRQ_INDEX  ||
            irq_index == ARCH_C28_YIELD_IRQ_INDEX ||
            irq_index == ARCH_C28_SW_IRQ_INDEX);
}

/* ----------------------------------------------------------------------------
 * IRQ management
 * For C28x, user interrupts come through PIE groups 1-12.
 * irq_index range: ARCH_C28_PIE_BASE_INDEX + (group-1)*8 + (channel-1)
 * --------------------------------------------------------------------------*/
KernelResult ArchIrqSetPriority(uint32_t irq_index, uint32_t priority)
{
    /* C28x has no runtime-programmable interrupt priority — fixed by PIE group */
    (void)irq_index;
    (void)priority;
    return kSuccess;
}

KernelResult ArchIrqEnable(uint32_t irq_index)
{
    uint32_t rel;
    uint32_t group;
    uint16_t channel_bit;

    if (irq_index < ARCH_C28_PIE_BASE_INDEX) {
        return kErrorInvalidParam;
    }

    rel   = irq_index - ARCH_C28_PIE_BASE_INDEX;
    group = (rel / 8U) + 1U;

    if (group < 1U || group > 12U) {
        return kErrorInvalidParam;
    }

    channel_bit = (uint16_t)(1U << (rel % 8U));
    ARCH_HWREG16(CONFIG_ARCH_PIECTRL_BASE + PIECTRL_O_IER(group)) |= channel_bit;

    return kSuccess;
}

KernelResult ArchIrqDisable(uint32_t irq_index)
{
    uint32_t rel;
    uint32_t group;
    uint16_t channel_bit;

    if (irq_index < ARCH_C28_PIE_BASE_INDEX) {
        return kErrorInvalidParam;
    }

    rel   = irq_index - ARCH_C28_PIE_BASE_INDEX;
    group = (rel / 8U) + 1U;

    if (group < 1U || group > 12U) {
        return kErrorInvalidParam;
    }

    channel_bit = (uint16_t)(1U << (rel % 8U));
    ARCH_HWREG16(CONFIG_ARCH_PIECTRL_BASE + PIECTRL_O_IER(group)) &= ~channel_bit;

    return kSuccess;
}

/* ----------------------------------------------------------------------------
 * Software IRQ (softirq) — uses DLOGINT (IFR bit 12)
 * --------------------------------------------------------------------------*/
void ArchSwIrqPend(void)
{
    __asm(" OR IFR, #0x1000");  /* Set DLOGINT bit in IFR */
    __asm(" NOP");
    __asm(" NOP");
}

void ArchSwIrqBind(void (*handler)(void))
{
    IrqBind(ARCH_C28_SW_IRQ_INDEX, handler);
    /* DLOGINT is enabled via OR IER, #0x1000 in ArchStartKernel asm */
}

void ArchTaskCleanup(TaskControBlock *task)
{
    (void)task;
}

#endif /* CONFIG_ARCH_C2000_C28 > 0 */
