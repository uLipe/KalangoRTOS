#include <arch.h>
#include "arch_arm7m_defs.h"

#ifdef CONFIG_ARCH_ARM_V7M

static uint32_t irq_nest_level = 0;
static uint32_t irq_lock_level = 0;
static uint32_t irq_saved_level = 0;
uint32_t isr_stack[CONFIG_ISR_STACK_SIZE/4] __attribute__((aligned(8)));

typedef struct {
#ifdef CONFIG_HAS_FLOAT
    uint32_t has_fpu_context;
#endif
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
#ifdef CONFIG_HAS_FLOAT
    uint32_t s16;
    uint32_t s17;
    uint32_t s18;
    uint32_t s19;
    uint32_t s20;
    uint32_t s21;
    uint32_t s22;
    uint32_t s23;
    uint32_t s24;
    uint32_t s25;
    uint32_t s26;
    uint32_t s27;
    uint32_t s28;
    uint32_t s29;
    uint32_t s30;
    uint32_t s31;
#endif
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
#ifdef CONFIG_HAS_FLOAT
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t s12;
    uint32_t s13;
    uint32_t s14;
    uint32_t s15;
    uint32_t fpcsr;
    uint32_t fp_reserved;
#endif
}ArmCortexStackFrame;

typedef struct {
#ifdef CONFIG_HAS_FLOAT
    uint32_t has_fpu_context;
#endif
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
}ArmCortexStackFrameInitial;


void SysTick_Handler(void) {
    ArchIsrEnter();
    ClockStep(1);
    ArchIsrLeave();
}

KernelResult ArchInitializeSpecifics() {

#ifdef CONFIG_HAS_FLOAT
    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));  
    FPU->FPCCR = ( 0x3UL << 30UL );        
#endif
    
    //Align stack to 8-byte boundary:
    SCB->CCR |= 0x200;

    //Sets priority of interrupts used by kernel:
    SCB->SHP[SHP_SVCALL_PRIO] =  0xFF -  CONFIG_IRQ_PRIORITY_LEVELS;
	SCB->SHP[SHP_PENDSV_PRIO] =  0xFF -  (CONFIG_IRQ_PRIORITY_LEVELS - 8);
	SCB->SHP[SHP_SYSTICK_PRIO]  = 0xFF - CONFIG_IRQ_PRIORITY_LEVELS;

    //Setup systick timer to generate interrupt at tick rate:
	SysTick->CTRL = 0x00;
	SysTick->LOAD = CONFIG_PLATFORM_SYS_CLOCK_HZ/CONFIG_TICKS_PER_SEC;
    SysTick->CTRL = 0x07;

    return kSuccess;
}

KernelResult ArchStartKernel(uint32_t to) {
    __asm volatile ("   svc #0 \n");
    return kSuccess;
}

KernelResult ArchNewTask(TaskControBlock *task, uint8_t *stack_base, uint32_t stack_size) {
    ASSERT_PARAM(task);
    ASSERT_PARAM(stack_base);
    ASSERT_PARAM(stack_size);

    ArchCriticalSectionEnter();

    //Stack must be aligned to to 8-byte boundary
    uint8_t *aligned_stack  = (uint8_t *)((uint32_t)(stack_base + stack_size - 1) & ~0x07);
    aligned_stack -= sizeof(ArmCortexStackFrameInitial);
    ArmCortexStackFrameInitial *frame = (ArmCortexStackFrameInitial *)(aligned_stack);

    frame->r0 = (uint32_t)task->arg1;
    frame->xpsr = 0x01000000;
    frame->lr = 0xFFFFFFFD;
    frame->pc = (uint32_t)task->entry_point;

#if CONFIG_HAS_FLOAT 
    frame->has_fpu_context = 0;
#endif    
    frame->r1 = 0xAAAAAAAA;
    frame->r2 = 0xAAAAAAAA;
    frame->r3 = 0xAAAAAAAA;
    frame->r4 = 0xAAAAAAAA;
    frame->r5 = 0xAAAAAAAA;
    frame->r6 = 0xAAAAAAAA;
    frame->r7 = 0xAAAAAAAA;
    frame->r8 = 0xAAAAAAAA;
    frame->r9 = 0xAAAAAAAA;
    frame->r10 = 0xAAAAAAAA;
    frame->r11 = 0xAAAAAAAA;
    task->stackpointer = aligned_stack;
    
    ArchCriticalSectionExit();
    return kSuccess;
}

KernelResult ArchCriticalSectionEnter() {
    if(irq_lock_level < 0xFFFFFFFF) {
        irq_lock_level++;
    }

    if(irq_lock_level == 1) {
        irq_saved_level = __get_PRIMASK();
        __disable_irq();
    }

    return kSuccess;
}

KernelResult ArchCriticalSectionExit() {
    if(irq_lock_level) {
        irq_lock_level--;
    }

    if(!irq_lock_level) {
        __set_PRIMASK(irq_saved_level);
    }

    return kSuccess;
}

KernelResult ArchYield() {
    SCB->ICSR |= (1<<28); 
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
    return ((__get_IPSR() != 0) ? true : false);
}

uint8_t ArchCountLeadZeros(uint32_t word) {
    return __builtin_clz(word);
}

#endif