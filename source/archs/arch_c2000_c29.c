#include <KalangoRTOS/arch.h>
#include <KalangoRTOS/macros.h>


#if CONFIG_ARCH_C2000 > 0

#include "arch_c2000_c29_defs.h"

static uint32_t irq_nest_level = 0;
static uint32_t irq_lock_level = 0;

KernelResult ArchInitializeSpecifics() {

#ifdef CONFIG_HAS_FLOAT
    //TODO CHECK if we need to enable the FPU before using it
    //In code composer studio the startup code seems to enable it.
#endif

    //Set the tick time interrupt:
    //CPUTimer_stopTimer(CPUTIMER2_TICK_TIMER_BASE);
    uint32_t tcrValue = ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) & (~CPUTIMER_TCR_TIF);
    ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) = tcrValue | CPUTIMER_TCR_TSS;

    //CPUTimer_setPeriod(CPUTIMER2_TICK_TIMER_BASE, ((uint32_t)((CONFIG_PLATFORM_SYS_CLOCK_HZ / CONFIG_TICKS_PER_SEC))));
    ARCH_HWREG(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_PRD) = ((CONFIG_PLATFORM_SYS_CLOCK_HZ / CONFIG_TICKS_PER_SEC) - 1U);

    //CPUTimer_setPreScaler(CPUTIMER2_TICK_TIMER_BASE, 0U);
    ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TPRH) = 0 >> 8U;
    ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TPR) = (0 & CPUTIMER_TPR_TDDR_M) ;

    //CPUTimer_reloadTimerCounter(CPUTIMER2_TICK_TIMER_BASE);
    tcrValue = ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) & (~CPUTIMER_TCR_TIF);
    ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) = tcrValue | CPUTIMER_TCR_TRB;

    //Todo is needed?
    //CPUTimer_setEmulationMode(CPUTIMER2_TICK_TIMER_BASE, CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);

    //CPUTimer_clearOverflowFlag(CPUTIMER2_TICK_TIMER_BASE);
    ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) |= CPUTIMER_TCR_TIF;

    //CPUTimer_enableInterrupt(CPUTIMER2_TICK_TIMER_BASE);
    tcrValue = ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) & (~CPUTIMER_TCR_TIF);
    ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) = tcrValue | CPUTIMER_TCR_TIE;

    // Now set the interrupt fro the tick timer:
    //Interrupt_disable(CPUTIMER2_TICK_TIMER_INT);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CTL_L(CPUTIMER2_TICK_TIMER_INT)) = 0U;

    //Interrupt_clearFlag(CPUTIMER2_TICK_TIMER_INT);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CTL_H(CPUTIMER2_TICK_TIMER_INT)) = PIPE_INT_CTL_H_FLAG_CLR;

    //Interrupt_clearOverflowFlag(CPUTIMER2_TICK_TIMER_INT);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CTL_H(CPUTIMER2_TICK_TIMER_INT)) =
                                        PIPE_INT_CTL_H_OVERFLOW_FLAG_CLR;

    //Interrupt_register(CPUTIMER2_TICK_TIMER_INT, &ArchTickIsr);
    ARCH_HWREG(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_VECT_ADDR(CPUTIMER2_TICK_TIMER_INT)) = (uint32_t)&ArchTickIsr;

    //Interrupt_setPriority(CPUTIMER2_TICK_TIMER_INT, CPUTIMER2_TICK_TIMER_INT_PRI);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CONFIG(CPUTIMER2_TICK_TIMER_INT)) = CPUTIMER2_TICK_TIMER_INT_PRI;

    //Interrupt_enable(CPUTIMER2_TICK_TIMER_INT);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CTL_L(CPUTIMER2_TICK_TIMER_INT)) = PIPE_INT_CTL_L_EN;

    //CPUTimer_startTimer(CPUTIMER2_TICK_TIMER_BASE);
    tcrValue = ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) & (~CPUTIMER_TCR_TIF);
    ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) = tcrValue | CPUTIMER_TCR_TRB;
    ARCH_HWREGH(CPUTIMER2_TICK_TIMER_BASE + CPUTIMER_O_TCR) &= ~CPUTIMER_TCR_TSS;

    //Set the SWI interrupt used to perform context switch:
    // Interrupt_disable(ARCH_TASK_SWITCH_INT);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CTL_L(ARCH_TASK_SWITCH_INT)) = 0U;

    // Interrupt_clearFlag(ARCH_TASK_SWITCH_INT);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CTL_H(ARCH_TASK_SWITCH_INT)) = PIPE_INT_CTL_H_FLAG_CLR;

    // Interrupt_clearOverflowFlag(ARCH_TASK_SWITCH_INT);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CTL_H(ARCH_TASK_SWITCH_INT)) =
                                        PIPE_INT_CTL_H_OVERFLOW_FLAG_CLR;

    // Interrupt_register(ARCH_TASK_SWITCH_INT, &ArchSwitchIsr);
    ARCH_HWREG(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_VECT_ADDR(ARCH_TASK_SWITCH_INT)) = (uint32_t)&ArchSwitchIsr;

    // Interrupt_setPriority(ARCH_TASK_SWITCH_INT, ARCH_TASK_SWITCH_INT_PRI);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CONFIG(ARCH_TASK_SWITCH_INT)) = ARCH_TASK_SWITCH_INT_PRI;

    // Interrupt_enable(ARCH_TASK_SWITCH_INT);
    ARCH_HWREGB(CONFIG_ARCH_PIPE_BASE + PIPE_O_INT_CTL_L(ARCH_TASK_SWITCH_INT)) = PIPE_INT_CTL_L_EN;

    // Enable PIPE GLobal interrupt.
    ARCH_HWREG(CONFIG_ARCH_PIPE_BASE + PIPE_O_GLOBAL_EN) = 0x3U | PIPE_GLOBAL_EN_KEY;

    return kSuccess;
}


KernelResult ArchNewTask(TaskControBlock *task, uint8_t *stack_base, uint32_t stack_size) {
    ASSERT_PARAM(task);
    ASSERT_PARAM(stack_base);
    ASSERT_PARAM(stack_size);

    ArchCriticalSectionEnter();
    
    uint32_t *frame = (uint32_t *)ALIGN((uint32_t)stack_base, 8);
    uint32_t base = 0;

    frame[base++] = (uint32_t)0; 
    frame[base++] = 0x07F90001; //Slot placeholders for alignment.
    frame[base++] = 0xABABABAB; // Save A14 here using the empty slot of RPC
    frame[base++] = ((uint32_t)task->entry_point) & 0xFFFFFFFEU; //Task entry point as RPC
    frame[base++] = 0x07F90001; // DSTS
    frame[base++] = 0x00020101; // ESTS

    //Fill the rest of registers, thank you  FreeRTOS.
    int i = 0;
#if (CONFIG_HAS_FLOAT > 0)    
    for(i = 0; i <= (ARCH_A_REGISTERS + ARCH_D_REGISTERS + ARCH_M_REGISTERS -2 -1); i++) {
#else 
    for(i = 0; i <= (ARCH_A_REGISTERS + ARCH_D_REGISTERS -2 -1); i++) {
#endif
        uint32_t value  = 0x55555555;
        if(i == ARCH_A4_REGISTER_POSITION) {
            value = (uint32_t)task->arg1;  // Function parameters are passed in A4.
        }
        frame[base + i] = value;
    }

    base += i ;    
    task->stackpointer = (uint8_t *)(frame + base);

    ArchCriticalSectionExit();
    return kSuccess;
}

KernelResult ArchCriticalSectionEnter() {
    if(irq_lock_level < 0xFFFFFFFF) {
        irq_lock_level++;
    }

    if(irq_lock_level == 1) {
        ARCH_DISABLE_INTERRUPTS();
    }

    return kSuccess;
}

KernelResult ArchCriticalSectionExit() {
    if(irq_lock_level) {
        irq_lock_level--;
    }

    if(!irq_lock_level) {
        ARCH_ENABLE_INTERRUPTS();
    }

    return kSuccess;
}

KernelResult ArchYield() {
    ArchSwitch();
    return kSuccess;
}

KernelResult ArchIsrEnter() {
    if(irq_nest_level < 0xFFFFFFFF) {
        irq_nest_level++;
    }

    return kSuccess;
}

KernelResult ArchIsrLeave() {
    if(irq_nest_level) {
        irq_nest_level--;
    }

    if(!irq_nest_level) {
        CheckReschedule();
    }

    return kSuccess;
}

uint32_t ArchGetIsrNesting() {
    return irq_nest_level;
}

bool ArchInIsr() {
    volatile uint32_t dsts;
   __asm__(" ST.32 *(A15-#8), DSTS");
   return (((dsts  &  (0x3 << 17)) != 0) ? true : false);
}

uint8_t ArchCountLeadZeros(uint32_t word) {
    return __builtin_c29_i32_clzeros_d( ( word ));
}


#endif