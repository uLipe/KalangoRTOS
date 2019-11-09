;;******************************************************************************
;;
;; arch_c2000_c28_asm.asm -- KalangoRTOS C28x context switch assembly
;;
;; Target: TMS320C28x (EABI, --silicon_version=28)
;; Assembler: TI Code Generation Tools (cl2000)
;;
;; Notes on C28x register conventions:
;;   - SP is 16-bit dedicated stack pointer (NOT part of XAR7 — they are separate)
;;   - Hardware auto-saves on interrupt entry (restored by IRET):
;;       T, ST0, AH, AL, PH, PL, AR0H, AR1H, DP, ST1, DBGSTAT, IER, PC
;;   - Manually saved registers: XAR0-XAR6, XT (full), RPC [+ FPU if present]
;;   - SP saved to TCB.stackpointer (as 32-bit via ACC intermediary)
;;
;;******************************************************************************

    .global ArchTickIsr
    .global ArchSwitchIsr
    .global ArchSwIrqIsr
    .global ArchStartKernel
    .global ArchPieGroup1Isr
    .global ArchPieGroup2Isr
    .global ArchPieGroup3Isr
    .global ArchPieGroup4Isr
    .global ArchPieGroup5Isr
    .global ArchPieGroup6Isr
    .global ArchPieGroup7Isr
    .global ArchPieGroup8Isr
    .global ArchPieGroup9Isr
    .global ArchPieGroup10Isr
    .global ArchPieGroup11Isr
    .global ArchPieGroup12Isr

    .ref    _current
    .ref    _CoreSetRunning
    .ref    _ClockStep
    .ref    _CoreTaskSwitch
    .ref    _CheckReschedule
    .ref    _ArchIsrEnter
    .ref    _ArchIsrLeave
    .ref    _ArchPieGroupDispatch
    .ref    _IrqDispatch

;;
;; CPU Timer 2 base address (C28x word address — standard across F28x)
;;
CPUTIMER2_BASE  .set    0x0C00
CPUTIMER_TCR_OFF .set   4        ; TCR offset in 16-bit words from timer base
CPUTIMER_TIF_BIT .set   0x8000

;;******************************************************************************
;; SAVE_CONTEXT — save caller-saved registers and store SP to current TCB.
;;
;; Hardware has already pushed (on interrupt entry): T, ST0, AH, AL, PH, PL,
;; AR0H, AR1H, DP, ST1, DBGSTAT, IER, PC.
;;
;; SP cannot be used directly as MOV indirect operand; use AL as intermediary.
;;******************************************************************************
SAVE_CONTEXT    .macro

    ;; Save RPC (used by LCR/RETL long calls)
    PUSH    RPC

    ;; Save full XT (T was auto-saved but XL lower half was not)
    PUSH    XT

    ;; Save XAR0 through XAR6
    PUSH    XAR0
    PUSH    XAR1
    PUSH    XAR2
    PUSH    XAR3
    PUSH    XAR4
    PUSH    XAR5
    PUSH    XAR6

    .if .TMS320C2800_FPU32
    MOV32   *--SP, R0H
    MOV32   *--SP, R1H
    MOV32   *--SP, R2H
    MOV32   *--SP, R3H
    MOV32   *--SP, R4H
    MOV32   *--SP, R5H
    MOV32   *--SP, R6H
    MOV32   *--SP, R7H
    MOV32   *--SP, STF
    PUSH    RB
    .endif

    ;; Store SP to TCB.stackpointer (first field of TCB at offset 0).
    ;; MOV *XARn, SP is invalid — use ACC (AH:AL) as intermediary.
    MOVL    XAR0, #_current         ;; XAR0 = &current
    MOVL    XAR0, *XAR0             ;; XAR0 = current (TCB pointer)
    MOV     AL, SP                  ;; AL = SP (16-bit value)
    MOV     AH, #0
    MOVL    *XAR0, ACC              ;; store 32-bit SP to TCB.stackpointer

    .endm


;;******************************************************************************
;; RESTORE_CONTEXT — load new task SP from TCB and restore caller-saved regs.
;; IRET (at end of ISR) will then restore the hardware auto-saved registers.
;;******************************************************************************
RESTORE_CONTEXT .macro

    ;; Load new task SP from TCB.stackpointer
    MOVL    XAR0, #_current
    MOVL    XAR0, *XAR0             ;; XAR0 = current (new TCB)
    MOVL    ACC, *XAR0              ;; ACC = stackpointer (32-bit; SP in AL)
    MOV     SP, AL                  ;; restore SP

    .if .TMS320C2800_FPU32
    POP     RB
    MOV32   STF, *SP++
    MOV32   R7H, *SP++
    MOV32   R6H, *SP++
    MOV32   R5H, *SP++
    MOV32   R4H, *SP++
    MOV32   R3H, *SP++
    MOV32   R2H, *SP++
    MOV32   R1H, *SP++
    MOV32   R0H, *SP++
    .endif

    POP     XAR6
    POP     XAR5
    POP     XAR4
    POP     XAR3
    POP     XAR2
    POP     XAR1
    POP     XAR0
    POP     XT
    POP     RPC

    .endm


;;******************************************************************************
;; ArchTickIsr — CPU Timer 2 tick interrupt (INT14, direct — no PIE)
;;******************************************************************************
ArchTickIsr:    .asmfunc
    SAVE_CONTEXT

    ;; Clear CPU Timer 2 TIF bit (write 1 to clear the overflow flag)
    MOV     AL, @(CPUTIMER2_BASE + CPUTIMER_TCR_OFF)
    OR      AL, #CPUTIMER_TIF_BIT
    MOV     @(CPUTIMER2_BASE + CPUTIMER_TCR_OFF), AL

    ;; ClockStep(1)
    MOV     AL, #1
    MOV     AH, #0
    LCR     #_ClockStep

    ;; Notify leave (may trigger reschedule)
    LCR     #_ArchIsrLeave

    RESTORE_CONTEXT
    IRET
    .endasmfunc


;;******************************************************************************
;; ArchSwitchIsr — RTOSINT handler (voluntary context switch)
;; Fired by: OR IFR, #0x2000 in ArchYield()
;;******************************************************************************
ArchSwitchIsr:  .asmfunc
    SAVE_CONTEXT
    LCR     #_CoreTaskSwitch
    RESTORE_CONTEXT
    IRET
    .endasmfunc


;;******************************************************************************
;; ArchSwIrqIsr — DLOGINT handler (software IRQ / softirq)
;; Fired by: OR IFR, #0x1000 in ArchSwIrqPend()
;; irq_index = ARCH_C28_SW_IRQ_INDEX = 12
;;******************************************************************************
ArchSwIrqIsr:   .asmfunc
    SAVE_CONTEXT
    LCR     #_ArchIsrEnter
    MOV     AL, #12
    MOV     AH, #0
    LCR     #_IrqDispatch
    LCR     #_ArchIsrLeave
    RESTORE_CONTEXT
    IRET
    .endasmfunc


;;******************************************************************************
;; ArchStartKernel — restore first context and begin scheduling.
;; Enables RTOSINT + DLOGINT in IER, then restores first task via IRET.
;;******************************************************************************
ArchStartKernel: .asmfunc
    LCR     #_CoreSetRunning

    ;; Enable RTOSINT (bit 13) and DLOGINT (bit 12) in IER
    OR      IER, #0x3000

    RESTORE_CONTEXT
    EINT
    IRET
    .endasmfunc


;;******************************************************************************
;; PIE Group ISR wrappers — one per PIE group (1-12).
;;
;; Each wrapper saves context, calls ArchPieGroupDispatch(group) which reads
;; the PIE IFR to identify the channel, ACKs the group, and dispatches to
;; IrqDispatch.  Then restores context.
;;
;; TI CG assembler macro: parameter accessed via \param (not :param:).
;;******************************************************************************
PIE_GROUP_ISR   .macro  gnum
    SAVE_CONTEXT
    LCR     #_ArchIsrEnter
    MOV     AL, #gnum
    MOV     AH, #0
    LCR     #_ArchPieGroupDispatch
    LCR     #_ArchIsrLeave
    RESTORE_CONTEXT
    IRET
    .endm

ArchPieGroup1Isr:  .asmfunc
    PIE_GROUP_ISR  1
    .endasmfunc

ArchPieGroup2Isr:  .asmfunc
    PIE_GROUP_ISR  2
    .endasmfunc

ArchPieGroup3Isr:  .asmfunc
    PIE_GROUP_ISR  3
    .endasmfunc

ArchPieGroup4Isr:  .asmfunc
    PIE_GROUP_ISR  4
    .endasmfunc

ArchPieGroup5Isr:  .asmfunc
    PIE_GROUP_ISR  5
    .endasmfunc

ArchPieGroup6Isr:  .asmfunc
    PIE_GROUP_ISR  6
    .endasmfunc

ArchPieGroup7Isr:  .asmfunc
    PIE_GROUP_ISR  7
    .endasmfunc

ArchPieGroup8Isr:  .asmfunc
    PIE_GROUP_ISR  8
    .endasmfunc

ArchPieGroup9Isr:  .asmfunc
    PIE_GROUP_ISR  9
    .endasmfunc

ArchPieGroup10Isr: .asmfunc
    PIE_GROUP_ISR  10
    .endasmfunc

ArchPieGroup11Isr: .asmfunc
    PIE_GROUP_ISR  11
    .endasmfunc

ArchPieGroup12Isr: .asmfunc
    PIE_GROUP_ISR  12
    .endasmfunc
