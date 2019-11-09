#include <KalangoRTOS/arch.h>

#if CONFIG_ARCH_TRICORE > 0

#include "arch_tricore_defs.h"
#include <KalangoRTOS/irq.h>
#include <KalangoRTOS/task.h>
#include <string.h>

/* Forward declaration from platform layer */
extern void platform_putchar(char c);

/* External symbols provided by the assembly layer */
extern void Arch_StartFirstTask(void);
extern void TaskReturnHook(void);
extern uint32_t _tricore_int_tab;
extern uint32_t _tricore_trap_tab;

/* ISR stack for the interrupt stack pointer (ISP) register */
static uint32_t isr_stack[CONFIG_ISR_STACK_SIZE / 4U + 8U];

/* CSA free list pool — in .csa_pool (NOLOAD) so BSS zeroing in platform_init
 * does not destroy the chain initialised by startup.S before any CALL.
 * Each CSA is 16 words (64 bytes), 64-byte aligned. */
uint32_t tricore_csa_pool[CONFIG_TRICORE_CSA_COUNT * 16U]
    __attribute__((aligned(64)))
    __attribute__((section(".csa_pool")));

/* Nesting counter for critical sections */
static uint32_t irq_lock_level;
static uint32_t irq_saved_ie;   /* saved ICR.IE bit */

/* ---------------------------------------------------------------------------
 * CSFR read/write helpers
 * --------------------------------------------------------------------------- */

static inline uint32_t arch_mfcr(uint32_t csfr)
{
    uint32_t v;
    __asm volatile("mfcr %0, %1" : "=d"(v) : "i"(csfr));
    return v;
}

static inline void arch_mtcr(uint32_t csfr, uint32_t v)
{
    __asm volatile("dsync\n\t"
                   "mtcr %1, %0\n\t"
                   "isync"
                   : : "d"(v), "i"(csfr) : "memory");
}


/* ---------------------------------------------------------------------------
 * TC27x STM tick initialisation
 *
 * STM0 channel 0 fires at CONFIG_TICKS_PER_SEC Hz. The SRN for STM0 SR0 is
 * set to ARCH_TRICORE_TICK_PRIORITY. Both STM and SRN configuration require
 * ENDINIT write protection to be lifted on real hardware; in QEMU this is a
 * direct MMIO write with no protection.
 * --------------------------------------------------------------------------- */
static volatile uint32_t dbg_tick_count;

static void arch_tick_isr(void)
{
    volatile uint32_t *stm = (volatile uint32_t *)TRICORE_STM0_BASE;

    /* Acknowledge: clear CMP0 interrupt flag */
    stm[TRICORE_STM_ISCR / 4U] = TRICORE_STM_ISCR_CMP0IRR;

    /* Advance compare value for next tick */
    stm[TRICORE_STM_CMP0 / 4U] += (uint32_t)(CONFIG_PLATFORM_SYS_CLOCK_HZ /
                                              CONFIG_TICKS_PER_SEC);

    dbg_tick_count++;
    if ((dbg_tick_count % 10U) == 0U) {
        platform_putchar('~');
    }

    ClockStep(1U);
}

static void arch_stm_init(void)
{
    volatile uint32_t *stm = (volatile uint32_t *)TRICORE_STM0_BASE;
    volatile uint32_t *srn = (volatile uint32_t *)TRICORE_SRC_STM0_SR0;

    /* Full 32-bit compare on TIM0, starting at bit 0 */
    stm[TRICORE_STM_CMCON / 4U] = TRICORE_STM_CMCON_VAL;

    /* Arm CMP0: current timer + one tick period */
    stm[TRICORE_STM_CMP0 / 4U] = stm[TRICORE_STM_TIM0 / 4U] +
                                  (uint32_t)(CONFIG_PLATFORM_SYS_CLOCK_HZ /
                                             CONFIG_TICKS_PER_SEC);

    /* Enable CMP0 interrupt */
    stm[TRICORE_STM_ICR / 4U] = TRICORE_STM_ICR_CMP0EN;

    /* Configure SRN: priority = tick priority, enabled */
    *srn = TRICORE_SRN_EN | ARCH_TRICORE_TICK_PRIORITY;

    IrqBind(ARCH_TRICORE_TICK_PRIORITY, arch_tick_isr);
}

/* ---------------------------------------------------------------------------
 * ArchInitializeSpecifics
 * --------------------------------------------------------------------------- */
KernelResult ArchInitializeSpecifics(void)
{
    /* VSS=1 (bit 0): 8-byte vector stride; QEMU: PC = BIV | (pipn << 3) */
    arch_mtcr(TRICORE_CSFR_BIV, (uint32_t)&_tricore_int_tab | 1U);
    arch_mtcr(TRICORE_CSFR_BTV, (uint32_t)&_tricore_trap_tab);
    uint32_t isp_top = ((uint32_t)(isr_stack) + CONFIG_ISR_STACK_SIZE) & ~7U;
    arch_mtcr(TRICORE_CSFR_ISP, isp_top);
    uint32_t syscon = arch_mfcr(TRICORE_CSFR_SYSCON);
    arch_mtcr(TRICORE_CSFR_SYSCON, syscon | 0x1U);
    IrqTableInit();
    volatile uint32_t *sw_srn = (volatile uint32_t *)TRICORE_SRC_CPU0_SR0;
    *sw_srn = TRICORE_SRN_EN | ARCH_TRICORE_SW_IRQ_PRIORITY;
    arch_stm_init();
    return kSuccess;
}

/* ---------------------------------------------------------------------------
 * ArchStartKernel
 * --------------------------------------------------------------------------- */
KernelResult ArchStartKernel(void)
{
    irq_lock_level = 0U;
    irq_saved_ie   = 0U;

    /* Disable CDC before first rfe so BISR/RFE during startup don't trap. */
    uint32_t psw = arch_mfcr(TRICORE_CSFR_PSW);
    psw &= ~0x80U;   /* CDE = 0 */
    arch_mtcr(TRICORE_CSFR_PSW, psw);
    Arch_StartFirstTask();
    return kSuccess;
}

/* ---------------------------------------------------------------------------
 * ArchNewTask
 *
 * TriCore does not use a traditional stack pointer for context tracking.
 * Instead, task->stackpointer stores a CSA link (uint32_t) to the task's
 * lower context CSA, which chains to the upper context CSA:
 *
 *   lower_csa[0] → upper CSA link (UL=1, PIE=1)
 *   lower_csa[1]  = A11 = task entry point (return address for RFE)
 *   lower_csa[8]  = A4  = task argument
 *
 *   upper_csa[0] = 0  (end of chain)
 *   upper_csa[1] = PSW (supervisor, CDC disabled)
 *   upper_csa[2] = A10 = stack pointer (top of the C stack)
 *   upper_csa[3] = A11 = entry point (captured by RFE as PC)
 *
 * The C stack occupies the full stack_base..stack_top region.
 * CSAs are allocated from the global pool (FCX), NOT from the task's stack.
 * --------------------------------------------------------------------------- */
KernelResult ArchNewTask(TaskControBlock *task, uint8_t *stack_base,
                         uint32_t stack_size)
{
    ASSERT_PARAM(task);
    ASSERT_PARAM(stack_base);
    ASSERT_PARAM(stack_size);

    ArchCriticalSectionEnter();

    /* C stack top (8-byte aligned) */
    uint32_t stack_top = ((uint32_t)(stack_base + stack_size)) & ~7U;

    /*
     * Three CSAs are required per task:
     *
     *   lower_csa      — initial lower context (freed by RSLCX on first start)
     *   inner_upper_csa — initial upper context (freed by RFE on first start)
     *   outer_upper_csa — permanent base context (lives until task terminates)
     *
     * After the first RSLCX+RFE, PCXI = link to outer_upper_csa (not 0).
     * This means the task function's final `ret` restores outer_upper_csa,
     * landing in TaskReturnHook rather than dereferencing PCXI=0 (DSE/CSU).
     */
    uint32_t fcx = arch_mfcr(TRICORE_CSFR_FCX);
    if (!fcx) {
        ArchCriticalSectionExit();
        return kErrorNotEnoughKernelMemory;
    }

    uint32_t *lower_csa = TRICORE_CSA_TO_ADDR(fcx);
    uint32_t fcx2       = lower_csa[0];
    if (!fcx2) {
        ArchCriticalSectionExit();
        return kErrorNotEnoughKernelMemory;
    }

    uint32_t *inner_upper_csa = TRICORE_CSA_TO_ADDR(fcx2);
    uint32_t fcx3             = inner_upper_csa[0];
    if (!fcx3) {
        ArchCriticalSectionExit();
        return kErrorNotEnoughKernelMemory;
    }

    uint32_t *outer_upper_csa = TRICORE_CSA_TO_ADDR(fcx3);
    uint32_t new_fcx          = outer_upper_csa[0];

    /* Advance FCX past all three consumed CSAs */
    arch_mtcr(TRICORE_CSFR_FCX, new_fcx);

    /* outer_upper_csa — persists for the task's lifetime.
     * When the task function executes its final `ret`, PCXI points here and
     * the CPU restores A11 = TaskReturnHook as the return PC. */
    memset(outer_upper_csa, 0, 16U * sizeof(uint32_t));
    outer_upper_csa[TRICORE_UCSA_PCXI] = 0U;
    outer_upper_csa[TRICORE_UCSA_PSW]  = TRICORE_INITIAL_PSW;
    outer_upper_csa[TRICORE_UCSA_A10]  = stack_top;
    outer_upper_csa[TRICORE_UCSA_A11]  = (uint32_t)TaskReturnHook;

    /* inner_upper_csa — consumed by the first RFE.
     * PC  = lower_csa[A11] = entry_point   (set by RSLCX before RFE).
     * A11 = inner_upper_csa[A11] = TaskReturnHook (set by RFE after PC is latched).
     * Every CALL in the task saves A11=TaskReturnHook; every RET restores it.
     * So the task's final implicit `ret` jumps to TaskReturnHook, not entry_point. */
    memset(inner_upper_csa, 0, 16U * sizeof(uint32_t));
    inner_upper_csa[TRICORE_UCSA_PCXI] = TRICORE_PCXI_UL | TRICORE_PCXI_PIE |
                                          TRICORE_ADDR_TO_CSA(outer_upper_csa);
    inner_upper_csa[TRICORE_UCSA_PSW]  = TRICORE_INITIAL_PSW;
    inner_upper_csa[TRICORE_UCSA_A10]  = stack_top;
    inner_upper_csa[TRICORE_UCSA_A11]  = (uint32_t)TaskReturnHook;

    /* lower_csa — links to inner_upper_csa; A11 is the entry PC for RFE. */
    memset(lower_csa, 0, 16U * sizeof(uint32_t));
    lower_csa[TRICORE_LCSA_PCXI] = TRICORE_PCXI_UL | TRICORE_PCXI_PIE |
                                    TRICORE_ADDR_TO_CSA(inner_upper_csa);
    lower_csa[TRICORE_LCSA_A11]  = (uint32_t)task->entry_point;
    lower_csa[TRICORE_LCSA_A4]   = (uint32_t)task->arg1;

    /* task->stackpointer = CSA link to the lower context */
    task->stackpointer = (uint8_t *)(uintptr_t)TRICORE_ADDR_TO_CSA(lower_csa);

    ArchCriticalSectionExit();
    return kSuccess;
}

/* ---------------------------------------------------------------------------
 * ArchTaskCleanup — return a terminated task's saved CSA chain to FCX
 *
 * When a task self-deletes, Arch_SyscallHandler saves its context (Lower+
 * Upper CSAs from svlcx + syscall entry, plus any CALL frames in the
 * TaskReturnHook→TaskDelete→ArchYield call chain) into task->stackpointer.
 * Those CSAs are never touched again, leaking them from the FCX pool.
 *
 * This function traverses the PCXI chain and returns every CSA to FCX.
 * Must be called from IdleTask before FreeRawBuffer/FreeTaskObject.
 * --------------------------------------------------------------------------- */
void ArchTaskCleanup(TaskControBlock *task)
{
    uint32_t pcxi = (uint32_t)(uintptr_t)task->stackpointer;

    ArchCriticalSectionEnter();

    while (pcxi) {
        uint32_t csa_link = pcxi & 0x000FFFFFU;   /* strip UL/PIE/PCPN */
        uint32_t *csa     = TRICORE_CSA_TO_ADDR(csa_link);
        uint32_t next     = csa[0];                /* next CSA in chain */

        /* Prepend this CSA back to the free list */
        uint32_t old_fcx = arch_mfcr(TRICORE_CSFR_FCX);
        csa[0] = old_fcx;
        arch_mtcr(TRICORE_CSFR_FCX, csa_link);

        pcxi = next;
    }

    ArchCriticalSectionExit();
}

/* ---------------------------------------------------------------------------
 * Critical section — nested, uses ICR.IE
 * --------------------------------------------------------------------------- */
KernelResult ArchCriticalSectionEnter(void)
{
    if (irq_lock_level < 0xFFFFFFFFU) {
        irq_lock_level++;
    }

    if (irq_lock_level == 1U) {
        irq_saved_ie = arch_mfcr(TRICORE_CSFR_ICR) & TRICORE_ICR_IE;
        __asm volatile("disable" ::: "memory");
    }

    return kSuccess;
}

KernelResult ArchCriticalSectionExit(void)
{
    if (irq_lock_level) {
        irq_lock_level--;
    }

    if (!irq_lock_level && irq_saved_ie) {
        __asm volatile("enable" ::: "memory");
    }

    return kSuccess;
}

/* ---------------------------------------------------------------------------
 * Yield — voluntary context switch via SYSCALL trap class 6, TIN=0
 *
 * If called from an ISR, the forced switch is handled at ISR exit in
 * Arch_CommonIsrEntry by CoreSwitchPending(); this is a no-op there.
 * --------------------------------------------------------------------------- */
KernelResult ArchYield(void)
{
    if (ArchInIsr()) {
        return kSuccess;
    }
    __asm volatile("syscall 0" ::: "memory");
    return kSuccess;
}

/* ---------------------------------------------------------------------------
 * ISR enter/leave — no-ops; nesting tracked by IrqDispatch in irq.c
 * --------------------------------------------------------------------------- */
KernelResult ArchIsrEnter(void)
{
    return kSuccess;
}

KernelResult ArchIsrLeave(void)
{
    return kSuccess;
}

uint32_t ArchGetIsrNesting(void)
{
    return IrqGetNestLevel();
}

/* ArchInIsr: ICR.CCPN > 0 means an interrupt is currently active */
bool ArchInIsr(void)
{
    return (arch_mfcr(TRICORE_CSFR_ICR) & TRICORE_ICR_CCPN_MASK) > 0U;
}

uint8_t ArchCountLeadZeros(uint32_t word)
{
    return (uint8_t)__builtin_clz(word);
}

/* ---------------------------------------------------------------------------
 * IRQ management — SRN (Service Request Node) configuration
 *
 * On TriCore, irq_index IS the interrupt priority (SRPN field in the SRN).
 * The SRN physical addresses are peripheral-specific; for QEMU TC27x targets
 * the platform layer configures hardware SRNs. The arch layer reserves:
 *   priority 1 = software IRQ (CPU_SRC0)
 *   priority 2 = tick (STM0 SR0)
 *   priority 3+ = user-installed handlers
 * --------------------------------------------------------------------------- */

bool ArchIrqDenyUserInstall(uint32_t irq_index)
{
    return (irq_index == ARCH_TRICORE_TICK_PRIORITY) ||
           (irq_index == ARCH_TRICORE_SW_IRQ_PRIORITY);
}

KernelResult ArchIrqSetPriority(uint32_t irq_index, uint32_t priority)
{
    /* On TriCore, irq_index and priority are the same concept (SRPN).
     * This is a no-op; priority is fixed at irq_index by convention. */
    (void)irq_index;
    (void)priority;
    return kSuccess;
}

KernelResult ArchIrqEnable(uint32_t irq_index)
{
    /* SRN enable is peripheral-specific; handled by the platform/driver layer.
     * The IrqDispatch table is the software mechanism; no hardware action here. */
    (void)irq_index;
    return kSuccess;
}

KernelResult ArchIrqDisable(uint32_t irq_index)
{
    (void)irq_index;
    return kSuccess;
}

/* ---------------------------------------------------------------------------
 * Software IRQ — triggers via CPU0 SR0 (priority ARCH_TRICORE_SW_IRQ_PRIORITY)
 * --------------------------------------------------------------------------- */
void ArchSwIrqPend(void)
{
    volatile uint32_t *srn = (volatile uint32_t *)TRICORE_SRC_CPU0_SR0;
    *srn |= TRICORE_SRN_SETR;
}

void ArchSwIrqBind(void (*handler)(void))
{
    IrqBind(ARCH_TRICORE_SW_IRQ_PRIORITY, handler);
}

#endif /* CONFIG_ARCH_TRICORE */
