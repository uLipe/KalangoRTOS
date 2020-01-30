#include <arch.h>

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

#if defined(CONFIG_ARCH_ARM_V7M_VARIANT_M3)

    typedef enum {
    /* -------------------  Cortex-M4 Processor Exceptions Numbers  ------------------- */
    Reset_IRQn                    = -15,              /*!<   1  Reset Vector, invoked on Power up and warm reset                 */
    NonMaskableInt_IRQn           = -14,              /*!<   2  Non maskable Interrupt, cannot be stopped or preempted           */
    HardFault_IRQn                = -13,              /*!<   3  Hard Fault, all classes of Fault                                 */
    MemoryManagement_IRQn         = -12,              /*!<   4  Memory Management, MPU mismatch, including Access Violation
                                                            and No Match                                                          */
    BusFault_IRQn                 = -11,              /*!<   5  Bus Fault, Pre-Fetch-, Memory Access Fault, other address/memory
                                                            related Fault                                                         */
    UsageFault_IRQn               = -10,              /*!<   6  Usage Fault, i.e. Undef Instruction, Illegal State Transition    */
    SVCall_IRQn                   =  -5,              /*!<  11  System Service Call via SVC instruction                          */
    DebugMonitor_IRQn             =  -4,              /*!<  12  Debug Monitor                                                    */
    PendSV_IRQn                   =  -2,              /*!<  14  Pendable request for system service                              */
    SysTick_IRQn     
    }IRQn_Type;

    #undef __CM4_REV                 
    #undef __MPU_PRESENT                                                                     
    #undef __NVIC_PRIO_BITS               
    #undef __Vendor_SysTickConfig         
    #undef __FPU_PRESENT

    #define __CM4_REV                 0x0001            /*!< Cortex-M4 Core Revision                                               */
    #define __MPU_PRESENT                  1            /*!< MPU present or not                                                    */
    #define __NVIC_PRIO_BITS               3            /*!< Number of Bits used for Priority Levels                               */
    #define __Vendor_SysTickConfig         0            /*!< Set to 1 if different SysTick Config is used                          */
    #define __FPU_PRESENT                  0            /*!< FPU present or not  */ 
    #include <core_cm3.h>

#elif defined(CONFIG_ARCH_ARM_V7M_VARIANT_M4)

    typedef enum {
    /* -------------------  Cortex-M4 Processor Exceptions Numbers  ------------------- */
    Reset_IRQn                    = -15,              /*!<   1  Reset Vector, invoked on Power up and warm reset                 */
    NonMaskableInt_IRQn           = -14,              /*!<   2  Non maskable Interrupt, cannot be stopped or preempted           */
    HardFault_IRQn                = -13,              /*!<   3  Hard Fault, all classes of Fault                                 */
    MemoryManagement_IRQn         = -12,              /*!<   4  Memory Management, MPU mismatch, including Access Violation
                                                            and No Match                                                          */
    BusFault_IRQn                 = -11,              /*!<   5  Bus Fault, Pre-Fetch-, Memory Access Fault, other address/memory
                                                            related Fault                                                         */
    UsageFault_IRQn               = -10,              /*!<   6  Usage Fault, i.e. Undef Instruction, Illegal State Transition    */
    SVCall_IRQn                   =  -5,              /*!<  11  System Service Call via SVC instruction                          */
    DebugMonitor_IRQn             =  -4,              /*!<  12  Debug Monitor                                                    */
    PendSV_IRQn                   =  -2,              /*!<  14  Pendable request for system service                              */
    SysTick_IRQn     
    }IRQn_Type;

    #undef __CM4_REV                 
    #undef __MPU_PRESENT                                                                     
    #undef __NVIC_PRIO_BITS               
    #undef __Vendor_SysTickConfig         
    #undef __FPU_PRESENT

    #define __CM4_REV                 0x0001            /*!< Cortex-M4 Core Revision                                               */
    #define __MPU_PRESENT                  1            /*!< MPU present or not                                                    */
    #define __NVIC_PRIO_BITS               3            /*!< Number of Bits used for Priority Levels                               */
    #define __Vendor_SysTickConfig         0            /*!< Set to 1 if different SysTick Config is used                          */
    #define __FPU_PRESENT                  1            /*!< FPU present or not  */ 
    #include <core_cm4.h>

#elif defined(CONFIG_ARCH_ARM_V7M_VARIANT_M7)
    #error "arch: unknown cortex M variant, check conf file"
#else
    #error "arch: unknown cortex M variant, check conf file"
#endif

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
    NVIC_SetPriority((IRQn_Type)SysTick_IRQn, CONFIG_IRQ_PRIORITY_LEVELS - 8);
    NVIC_SetPriority((IRQn_Type)SVCall_IRQn, CONFIG_IRQ_PRIORITY_LEVELS - 8);
    NVIC_SetPriority((IRQn_Type)PendSV_IRQn, CONFIG_IRQ_PRIORITY_LEVELS - 1);

    SysTick_Config(CONFIG_PLATFORM_SYS_CLOCK_HZ/CONFIG_TICKS_PER_SEC);

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
    return __CLZ(word);
}

#endif