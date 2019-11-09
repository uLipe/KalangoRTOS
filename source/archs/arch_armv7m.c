#include <KalangoRTOS/arch.h>

#if CONFIG_ARCH_ARM_V7M > 0

#include "arch_arm7m_defs.h"

static uint32_t irq_nest_level = 0;
static uint32_t irq_lock_level = 0;
static uint32_t irq_saved_level = 0;
uint32_t isr_stack[CONFIG_ISR_STACK_SIZE/4 + 8];
uint8_t *isr_top_of_stack;

typedef struct {
#if (CONFIG_HAS_FLOAT > 0)
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
#if (CONFIG_HAS_FLOAT > 0)
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
#if (CONFIG_HAS_FLOAT > 0)
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
#if (CONFIG_HAS_FLOAT > 0)
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

void __attribute__((naked)) PendSV_Handler(void) {
    asm volatile(
        /* disable interrupts during context switch */
        "mrs r2, PRIMASK \n\r"
        "cpsid I \n\r"

        /* Check if this is the first switch, skip register saving if does */
        "ldr r0, =current \n\r"
        "ldr r0, [r0] \n\r"
        "mrs r1, psp \n\r"

        /* push all registers to stack, incluing fp ones if needed */
#if (CONFIG_HAS_FLOAT > 0)
        "tst lr, #0x10 \n\r"
        "it  eq \n\r"
        "vstmdbeq r1!, {d8 - d15} \n\r"
#endif

        "stmfd r1!, {r4 - r11} \n\r"

        /* for fp context we need to store that there are a fp active context */
#if (CONFIG_HAS_FLOAT > 0)
        "mov r4, #0x00 \n\r"
        "tst lr, #0x10 \n\r"
        "it  eq \n\r"
        "moveq r4, #0x01 \n\r"
        "stmfd r1!, {r4} \n\r"
#endif
        /* send stackpointer back to the tcb */
        "str r1, [r0] \n\r"

        "push {lr} \n\r"
        "bl CoreTaskSwitch \n\r"
        "pop {lr} \n\r"
        "ldr r1, [r0] \n\r"

        /* same here, if a fp context was active, restore the fp registers */
#if (CONFIG_HAS_FLOAT > 0)
        "ldmfd r1!, {r3} \n\r"
#endif
        /* ...after the callee regular registers */
        "ldmfd r1!, {r4 - r11} \n\r"

#if (CONFIG_HAS_FLOAT > 0)
        "cmp r3, #0x00 \n\r"
        "it ne \n\r"
        "vldmiane r1!, {d8 - d15} \n\r"
#endif
        /* horray the stack pointer is now handled to the CPU */
        "msr psp, r1 \n\r"

        /* if the previous context saving was FP we need to tell the CPU to resume it*/
#if (CONFIG_HAS_FLOAT > 0)
        "orr lr, lr, #0x10 \n\r"
        "cmp r3, #0x00 \n\r"
        "it ne \n\r"
        "bicne lr, lr, #0x10 \n\r"
#endif

        /* re-enable interrupts and ensure return in thumb mode */
        "orr lr, lr, #0x04 \n\r"
        "msr PRIMASK,r2 \n\r"
        "bx lr \n\r"
    );
}

void __attribute__((naked)) SVC_Handler(void) {
    asm volatile(
        "ldr r0, =isr_top_of_stack \n\r"
        "ldr r0, [r0] \n\r"
        "msr msp, r0 \n\r"

        "push {lr} \n\r"
        "bl CoreTaskSwitch \n\r"
        "pop {lr} \n\r"
        "ldr r1, [r0] \n\r"

#if (CONFIG_HAS_FLOAT > 0)
        "ldmfd r1!, {r3} \n\r"
#endif
        /* ...after the callee regular registers */
        "ldmfd r1!, {r4 - r11} \n\r"

        /* horray the stack pointer is now handled to the CPU */
        "msr psp, r1 \n\r"

        "push {r2,lr} \n\r"
        "bl CoreSetRunning \n\r"
        "pop {r2,lr} \n\r"

        /* re-enable interrupts and ensure return in thumb mode */
        "orr lr, lr, #0x04 \n\r"
        "bx lr \n\r"
    );
}

KernelResult ArchInitializeSpecifics() {

#if (CONFIG_HAS_FLOAT > 0)
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

    //Setup global isr stack pointer, align into a 8byte boundary:
    isr_top_of_stack  = (uint8_t *)((uint32_t)(isr_stack + (CONFIG_ISR_STACK_SIZE/4)) & ~0x07);

    return kSuccess;
}

KernelResult ArchStartKernel(uint32_t to) {
    __asm volatile ("svc #0 \n");
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

#if (CONFIG_HAS_FLOAT > 0)
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