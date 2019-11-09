    #include "KalangoRTOS/kalango_config_internal.h"

#define CPUTIMER2_TICK_TIMER_BASE           0x3021A000U
#define ARCH_TICK_TIMER_O_TCR               (CPUTIMER2_TICK_TIMER_BASE + 0x8U)

#if CONFIG_ARCH_C2000 > 0

    .extern current
    .extern CoreSetRunning
    .extern ClockStep
    .extern CoreTaskSwitch
    .extern ArchIsrEnter
    .extern ArchIsrLeave
    .global ArchTickIsr
    .global ArchSwitchIsr
    .global ArchStartKernel


    .macro ARCH_SAVE_CONTEXT
        ST.32 *(A15++#8), A14
        || MV A14, RPC
        ST.32 *(A15-#4), A14
        ST.32 *(A15++#8), DSTS
        ST.32 *(A15-#4), ESTS
        ST.64 *(A15++#8), XA0
        ST.64 *(A15++#8), XA2
        ST.64 *(A15++#8), XA4
        ST.64 *(A15++#8), XA6
        ST.64 *(A15++#8), XA8
        ST.64 *(A15++#8), XA10
        ST.64 *(A15++#8), XA12
        ST.64 *(A15++#8), XD0
        ST.64 *(A15++#8), XD2
        ST.64 *(A15++#8), XD4
        ST.64 *(A15++#8), XD6
        ST.64 *(A15++#8), XD8
        ST.64 *(A15++#8), XD10
        ST.64 *(A15++#8), XD12
        ST.64 *(A15++#8), XD14

#if CONFIG_ARCH_C2000_HAS_FLOAT > 0
        ST.64 *(A15++#8), XM0
        ST.64 *(A15++#8), XM2
        ST.64 *(A15++#8), XM4
        ST.64 *(A15++#8), XM6
        ST.64 *(A15++#8), XM8
        ST.64 *(A15++#8), XM10
        ST.64 *(A15++#8), XM12
        ST.64 *(A15++#8), XM14
        ST.64 *(A15++#8), XM16
        ST.64 *(A15++#8), XM18
        ST.64 *(A15++#8), XM20
        ST.64 *(A15++#8), XM22
        ST.64 *(A15++#8), XM24
        ST.64 *(A15++#8), XM26
        ST.64 *(A15++#8), XM28
        ST.64 *(A15++#8), XM30
#endif
        ; Store the new top of stack for the task.
        LD.32	A14, @current
        ST.32	*A14, A15
    .endm

    .macro ARCH_RESTORE_CONTEXT
        LD.32 A0, @current
        LD.32 A15, *A0

#if CONFIG_ARCH_C2000_HAS_FLOAT > 0
        LD.64 XM30, *(A15-=#8)
        LD.64 XM28, *(A15-=#8)
        LD.64 XM26, *(A15-=#8)
        LD.64 XM24, *(A15-=#8)
        LD.64 XM22, *(A15-=#8)
        LD.64 XM20, *(A15-=#8)
        LD.64 XM18, *(A15-=#8)
        LD.64 XM16, *(A15-=#8)
        LD.64 XM14, *(A15-=#8)
        LD.64 XM12, *(A15-=#8)
        LD.64 XM10, *(A15-=#8)
        LD.64 XM8,  *(A15-=#8)
        LD.64 XM6,  *(A15-=#8)
        LD.64 XM4,  *(A15-=#8)
        LD.64 XM2,  *(A15-=#8)
        LD.64 XM0,  *(A15-=#8)
#endif
        LD.64 XD14, *(A15-=#8)
        LD.64 XD12, *(A15-=#8)
        LD.64 XD10, *(A15-=#8)
        LD.64 XD8,  *(A15-=#8)
        LD.64 XD6,  *(A15-=#8)
        LD.64 XD4,  *(A15-=#8)
        LD.64 XD2,  *(A15-=#8)
        LD.64 XD0,  *(A15-=#8)
        LD.64 XA12, *(A15-=#8)
        LD.64 XA10, *(A15-=#8)
        LD.64 XA8,  *(A15-=#8)
        LD.64 XA6,  *(A15-=#8)
        LD.64 XA4,  *(A15-=#8)
        LD.64 XA2,  *(A15-=#8)
        LD.64 XA0,  *(A15-=#8)
        LD.32 ESTS, *(A15-#4)
        LD.32 DSTS, *(A15-=#8)
        LD.32 RPC,  *(A15-#4)
        LD.32 A14,  *(A15-=#8)

    .endm

    .macro ARCH_TICK_TIMER_CLEAR_OVERFLOW
        ; Clear CPUTIMER overflow bit
        MV  A4, #ARCH_TICK_TIMER_O_TCR
        LD.S16  D0, *(A4)
        OR      D0, #0x00008000
        ST.W0 *(A4), D0

    .endm

ArchTickIsr:
	; Save the context of the current task.
    ARCH_SAVE_CONTEXT
    ; Clear tick timer and increment tick counter
    CALL    @ArchIsrEnter
    ARCH_TICK_TIMER_CLEAR_OVERFLOW
    MV      D0, #1
    CALL    @ClockStep
    CALL    @ArchIsrLeave
    ; Restore the context of the task selected to execute.
    ARCH_RESTORE_CONTEXT

    RETI.INT


ArchSwitchIsr:
	; Save the context of the current task.
    ARCH_SAVE_CONTEXT

    ; Select the next task to execute.
    CALL      @CoreTaskSwitch

    ; Restore the context of the task selected to execute.
    ARCH_RESTORE_CONTEXT
    RETI.INT


ArchStartKernel:
    CALL @CoreSetRunning
    ARCH_RESTORE_CONTEXT
    ENINT
    RET

#endif