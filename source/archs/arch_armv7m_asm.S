.syntax unified
.thumb

#include <KalangoRTOS/kalango_config_internal.h>

#if CONFIG_ARCH_ARM_V7M > 0

#if CONFIG_HAS_FLOAT > 0
.cpu cortex-m4
.fpu fpv4-sp-d16
#else
.cpu cortex-m3
#endif

.extern current
.extern isr_top_of_stack
.extern CoreSwitchPending
.extern IrqDispatch

.global Arch_CommonIsrEntry
.global Arch_SvcHandler
.global Arch_StartFirstTask

.equ SCB_ICSR,        0xE000ED04
.equ ICSR_RETTOBASE,  0x00000800   //bit 11: set when no other exception is active

.equ SVC_YIELD,       0
.equ SVC_SOFTIRQ,     1
.equ ARCH_SVC_IRQ_INDEX, 11

/*
 * Per-task stack layout (lowest address first, task->stackpointer points at r4):
 *
 *   [s16..s31]                          FP callee regs (only when EXC_RETURN bit4==0)
 *   [r4 r5 r6 r7 r8 r9 r10 r11 lr]     callee regs + per-task EXC_RETURN
 *   [r0 r1 r2 r3 r12 lr pc xpsr]        hardware exception frame
 *
 * Two context-switch modes:
 *
 *   Voluntary (SVC #0): the task calls ArchYield -> svc #0. SVC entry saves
 *   callee regs + EXC_RETURN below the hardware frame that the CPU pushed, then
 *   calls CoreSwitchPending and, if a new task is available, updates current and
 *   PSP before exception return.
 *
 *   Forced (ISR exit): Arch_CommonIsrEntry dispatches the IRQ handler, then
 *   checks RETTOBASE (SCB->ICSR bit 11). At the outermost exception level
 *   (RETTOBASE==1) it calls CoreSwitchPending and, if a switch is due, saves the
 *   interrupted task's callee regs below the already-pushed hardware frame and
 *   restores the new task's frame before exception return.  Nested ISRs skip this
 *   step entirely; the outermost one always does the final check.
 *
 * EXC_RETURN is saved/restored per task so the hardware unstacks the correct
 * frame size (basic vs extended with FP) when returning to each task.
 */

/* -----------------------------------------------------------------------
 * Arch_SvcHandler  — SVCall exception handler
 *
 * Dispatches on the SVC immediate byte read from the stacked PC:
 *   SVC #0 (SVC_YIELD)   — voluntary context switch
 *   SVC #1 (SVC_SOFTIRQ) — trigger a software IRQ vector
 * ----------------------------------------------------------------------- */
.thumb_func
Arch_SvcHandler:
    cpsid i

    //Decode SVC immediate from stacked PC ([PSP+24] in a basic exception frame).
    //SVC is a 16-bit Thumb instruction at address A:  byte[A]=imm8, byte[A+1]=0xDF.
    //Stacked PC = A+2, so imm8 is at [stacked_PC - 2].
    mrs r0, psp
    ldr r1, [r0, #24]           //stacked PC (return address after SVC instruction)
    ldrb r1, [r1, #-2]          //imm8 of "SVC #imm": lower byte of the 16-bit encoding

    cmp r1, #SVC_YIELD
    beq .L_svc_yield
    cmp r1, #SVC_SOFTIRQ
    beq .L_svc_softirq
    cpsie i
    bx lr                       //unknown SVC: return without switch

.L_svc_yield:
    //Save old task's callee regs + EXC_RETURN below the hardware frame on PSP.
#if CONFIG_HAS_FLOAT > 0
    tst lr, #0x10
    it eq
    vstmdbeq r0!, {s16-s31}
#endif
    stmdb r0!, {r4-r11, lr}     //r0 = updated PSP = top of saved frame
    ldr r2, =current
    ldr r3, [r2]
    str r0, [r3]                //old->stackpointer = saved PSP (before C call clobbers r0)
    bl CoreSwitchPending        //r0 = new task or NULL
    b .L_svc_commit

.L_svc_softirq:
    //SoftIrqPend() already set the bitmap before issuing svc #1.
    //Save old task's context, then dispatch to the installed SW-IRQ handler
    //(SwIRQHandler) through the normal IrqDispatch path so that irq_nest_level
    //is bumped and ArchInIsr() stays true while the handler runs.
#if CONFIG_HAS_FLOAT > 0
    tst lr, #0x10
    it eq
    vstmdbeq r0!, {s16-s31}
#endif
    stmdb r0!, {r4-r11, lr}
    ldr r2, =current
    ldr r3, [r2]
    str r0, [r3]

    mov r0, #ARCH_SVC_IRQ_INDEX
    bl IrqDispatch              //calls SwIRQHandler → gives softirq_sem
    bl CoreSwitchPending        //r0 = new task or NULL (likely kSoftIrqD)
    b .L_svc_commit

    //Common epilogue: r0 = new_task (or NULL); old SP already stored in current->sp.
.L_svc_commit:
    cmp r0, #0
    beq .L_svc_restore

    ldr r2, =current
    str r0, [r2]                //current = new task
    ldr r1, [r0]                //new->stackpointer
    ldmia r1!, {r4-r11, lr}
#if CONFIG_HAS_FLOAT > 0
    tst lr, #0x10
    it eq
    vldmiaeq r1!, {s16-s31}
#endif
    msr psp, r1
    cpsie i
    bx lr                       //exception return to new task

.L_svc_restore:
    //No switch: reload old task's saved SP (stored by the entry paths above).
    ldr r2, =current
    ldr r3, [r2]
    ldr r1, [r3]
    ldmia r1!, {r4-r11, lr}
#if CONFIG_HAS_FLOAT > 0
    tst lr, #0x10
    it eq
    vldmiaeq r1!, {s16-s31}
#endif
    msr psp, r1
    cpsie i
    bx lr                       //exception return back to old task at yield point

/* -----------------------------------------------------------------------
 * Arch_CommonIsrEntry  — common entry for all non-SVC exceptions
 *
 * Dispatches the IRQ handler, then performs a forced in-place context switch
 * at the outermost exception level (RETTOBASE==1) if CoreSwitchPending says
 * one is due.  Nested ISRs return normally; the outermost one always does the
 * final reschedule check.
 * ----------------------------------------------------------------------- */
.thumb_func
Arch_CommonIsrEntry:
    push {r0, lr}               //save EXC_RETURN; r0 pads to 8-byte alignment
    mrs r0, IPSR
    bl IrqDispatch

    //Skip switch if other exceptions are still active (we are a nested ISR).
    ldr r1, =SCB_ICSR
    ldr r1, [r1]
    tst r1, #ICSR_RETTOBASE
    beq .L_isr_done             //RETTOBASE==0 → nested, skip

    bl CoreSwitchPending        //r0 = new task or NULL
    cmp r0, #0
    beq .L_isr_done

    //Forced switch: perform the switch before exception return so that
    //the CPU unwinds the new task's frame, not the interrupted one.
    pop {r1, lr}                //r1 discard; lr = old task's EXC_RETURN
    mrs r1, psp                 //r1 = old task's PSP (hw frame already pushed by CPU)
#if CONFIG_HAS_FLOAT > 0
    tst lr, #0x10
    it eq
    vstmdbeq r1!, {s16-s31}
#endif
    stmdb r1!, {r4-r11, lr}    //save callee regs + EXC_RETURN below hw frame
    ldr r2, =current
    ldr r3, [r2]
    str r1, [r3]                //old->stackpointer = saved PSP
    str r0, [r2]                //current = new task  (r0 still holds CoreSwitchPending result)
    ldr r1, [r0]                //new->stackpointer
    ldmia r1!, {r4-r11, lr}
#if CONFIG_HAS_FLOAT > 0
    tst lr, #0x10
    it eq
    vldmiaeq r1!, {s16-s31}
#endif
    msr psp, r1                 //CPU will unstack new task's hw frame on exception return
    bx lr                       //exception return to new task

.L_isr_done:
    pop {r0, lr}
    bx lr

/* -----------------------------------------------------------------------
 * Arch_StartFirstTask  — enter the first task from privileged thread mode
 *
 * Loads the initial stack frame and begins execution.  Uses a manual frame
 * unwind (not exception return) because there is no previous exception to
 * return from.  Never returns.
 * ----------------------------------------------------------------------- */
.thumb_func
Arch_StartFirstTask:
    ldr r0, =isr_top_of_stack
    ldr r0, [r0]
    msr msp, r0

    ldr r0, =current
    ldr r0, [r0]
    cbz r0, .L_start_hang

    ldr r1, [r0]                //task->stackpointer → r4 of initial frame
    ldmia r1!, {r4-r11}         //restore r4-r11 (no FP for the initial switch)
    adds r1, r1, #4             //skip the EXC_RETURN word in the initial frame
    msr psp, r1                 //PSP now at the base of the initial hw frame
    movs r2, #2
    msr control, r2             //switch to PSP in thread mode
    isb

    //Manually unwind the initial hardware frame:
    //  [PSP+0]  r0   [PSP+4]  r1   [PSP+8]  r2  [PSP+12] r3
    //  [PSP+16] r12  [PSP+20] lr   [PSP+24] pc  [PSP+28] xpsr
    pop {r0-r3}                 //consume r0-r3; PSP now at r12 slot
    ldr lr, [sp, #4]            //stacked lr = TaskReturnHook (Thumb bit already set)
    ldr r12, [sp, #8]           //stacked pc = entry (halfword-aligned, bit 0 clear)
    orr r12, r12, #1            //add Thumb bit required by bx
    add sp, sp, #16             //consume r12/lr/pc/xpsr
    cpsie i
    bx r12                      //branch to task entry point

.L_start_hang:
    b .L_start_hang

#endif
