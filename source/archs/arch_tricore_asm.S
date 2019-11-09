/*
 * TriCore TC1.6 / TC1.6.1 / TC1.6.2 kernel assembly
 *
 * AURIX GCC assembler syntax:
 *   - Data registers:    %d0 … %d15
 *   - Address registers: %a0 … %a15
 *   - No '#' prefix for immediates; just raw constants
 *   - Symbol relocations: @hi(sym) and @lo(sym)
 *   - High/low load pair: movh.a %aN, @hi(sym) + lea %aN, [%aN]@lo(sym)
 *
 * Context switch mechanism:
 *
 *   Voluntary (SYSCALL 0 from task):
 *     - Trap class 6 entry: hardware saves UPPER context, PCXI → upper_csa
 *     - Arch_SyscallHandler: SVLCX saves lower, PCXI → lower_csa → upper_csa
 *     - Save PCXI to current->stackpointer
 *     - CoreSwitchPending → new task
 *     - Load new->stackpointer into PCXI; RSLCX + RFE → new task
 *
 *   Forced (ISR exit):
 *     - Interrupt delivery: hardware saves UPPER, PCXI → upper_csa
 *     - BISR in vector: saves LOWER, PCXI → lower_bisr_csa → upper_csa
 *     - Arch_CommonIsrEntry: IrqDispatch + CoreSwitchPending
 *     - If switch: save PCXI → current->stackpointer, load new->stackpointer
 *     - RSLCX + RFE → new task (or original task)
 *
 *   RFE semantics (confirmed from QEMU op_helper.c):
 *     1. PC = CURRENT A11 (captured before upper context restore)
 *     2. Restore upper context from PCXI: PSW, A10-A15, D8-D15
 *     So RSLCX's restored A11 (= task's PC) is used as the branch target.
 *
 * Calling convention (TriCore EABI):
 *   Integer args:  D4, D5, D6, D7
 *   Address args:  A4, A5, A6, A7
 *   Integer ret:   D2
 *   Pointer ret:   A2
 */

#include "arch_tricore_defs.h"
#include <KalangoRTOS/kalango_config_internal.h>

#if CONFIG_ARCH_TRICORE > 0

    .extern current
    .extern IrqDispatch
    .extern CoreSwitchPending

/* ===========================================================================
 * Interrupt vector table  (BIV-based)
 *
 * Each entry is 8 bytes (4-byte BISR + 4-byte J):
 *   BISR prio   — saves lower context, sets ICR.CCPN = prio
 *   J    Arch_CommonIsrEntry
 *
 * Entry N is at: BIV + N * 8
 * Priority 0 is unused (means "not in ISR").
 * =========================================================================== */
    .section .inttab, "ax"
    .global _tricore_int_tab
    .balign 8
_tricore_int_tab:

    .set _prio, 0
    .rept 256
    bisr _prio
    j    Arch_CommonIsrEntry
    .fill 2, 1, 0           /* pad to 8 bytes; bisr=2B + j=4B + 2B pad = 8B */
    .set _prio, _prio + 1
    .endr

/* ===========================================================================
 * Trap vector table  (BTV-based)
 *
 * Each entry is 32 bytes (one J instruction + 28 bytes padding).
 * Trap class 6 = SYSCALL (yield / softirq).
 * =========================================================================== */
    .section .traptab, "ax"
    .global _tricore_trap_tab
    .balign 32
_tricore_trap_tab:
    /* Class 0: MMU     — mov(2B) + j(4B) + fill(26B) = 32B */
    mov     %d14, 0
    j    Arch_DefaultTrapHandler
    .fill 26, 1, 0
    /* Class 1: Internal protection */
    mov     %d14, 1
    j    Arch_DefaultTrapHandler
    .fill 26, 1, 0
    /* Class 2: Instruction errors */
    mov     %d14, 2
    j    Arch_DefaultTrapHandler
    .fill 26, 1, 0
    /* Class 3: Context management */
    mov     %d14, 3
    j    Arch_DefaultTrapHandler
    .fill 26, 1, 0
    /* Class 4: System bus & peripheral */
    mov     %d14, 4
    j    Arch_DefaultTrapHandler
    .fill 26, 1, 0
    /* Class 5: Assert */
    mov     %d14, 5
    j    Arch_DefaultTrapHandler
    .fill 26, 1, 0
    /* Class 6: SYSCALL  ← yield and softirq */
    j    Arch_SyscallHandler
    .fill 28, 1, 0
    /* Class 7: NMI */
    mov     %d14, 7
    j    Arch_DefaultTrapHandler
    .fill 26, 1, 0

/* ===========================================================================
 * Arch_DefaultTrapHandler — unhandled traps: emit '!' + TIN to VIRT then loop
 * =========================================================================== */
    .section .text
    .global Arch_DefaultTrapHandler
    .type   Arch_DefaultTrapHandler, @function
/*
 * Print %d1 as 8 hex nibbles to VIRT putchar at %a4.
 * Trashes %d0, %d2.  Must not use CALL (to avoid CSA dependency in trap context).
 *
 * extr.u Dd, Ds, pos5, width5  — zero-extended bit-field extract (immediate pos).
 * jge    Dd, const4, label     — jump if Dd >= const4 (signed); const4 ∈ [-8..7].
 * Using const4=0 to check sign after subtracting 10.
 */
.macro pnib shift
    extr.u  %d0, %d1, \shift, 4    /* d0 = (d1 >> shift) & 0xF */
    mov     %d2, %d0
    addi    %d2, %d2, -10           /* d2 = d0 - 10 */
    jge     %d2, 0, 993f            /* if d0 >= 10: alpha */
    addi    %d0, %d0, 0x30          /* '0'..'9' */
    j       994f
993:
    addi    %d0, %d0, 0x37          /* 'A'..'F'  (0x37 = 'A' - 10) */
994:
    st.w    [%a4], %d0
.endm

Arch_DefaultTrapHandler:
    movh.a  %a4, 0xBF00
    lea     %a4, [%a4] 0x0020               /* TRICORE_VIRT_PUTCHAR */
    mov     %d0, 0x21                        /* '!' */
    st.w    [%a4], %d0
    addi    %d0, %d14, 0x30                  /* CLASS + '0' */
    st.w    [%a4], %d0
    mov     %d0, 0x2F                        /* '/' */
    st.w    [%a4], %d0
    addi    %d0, %d15, 0x30                  /* TIN + '0' */
    st.w    [%a4], %d0
    mov     %d0, 0x20                        /* ' ' */
    st.w    [%a4], %d0
    /* "P=XXXXXXXX" — PCXI */
    mov     %d0, 0x50                        /* 'P' */
    st.w    [%a4], %d0
    mov     %d0, 0x3D                        /* '=' */
    st.w    [%a4], %d0
    mfcr    %d1, TRICORE_CSFR_PCXI
    pnib 28
    pnib 24
    pnib 20
    pnib 16
    pnib 12
    pnib 8
    pnib 4
    pnib 0
    mov     %d0, 0x20                        /* ' ' */
    st.w    [%a4], %d0
    /* "F=XXXXXXXX" — FCX */
    mov     %d0, 0x46                        /* 'F' */
    st.w    [%a4], %d0
    mov     %d0, 0x3D                        /* '=' */
    st.w    [%a4], %d0
    mfcr    %d1, TRICORE_CSFR_FCX
    pnib 28
    pnib 24
    pnib 20
    pnib 16
    pnib 12
    pnib 8
    pnib 4
    pnib 0
    mov     %d0, 0x0A                        /* '\n' */
    st.w    [%a4], %d0
.L_trap_hang:
    j    .L_trap_hang

/* ===========================================================================
 * Arch_SyscallHandler — trap class 6 handler
 *
 * On entry:
 *   Hardware already saved upper context; PCXI → upper_csa (UL=1)
 *   %d15 = TIN: 0 = yield, 1 = softirq
 *
 * A10 is the interrupted task's stack (from the saved upper context).
 * ISP is used only for interrupt (BISR) entries, not trap entries.
 * =========================================================================== */
    .global Arch_SyscallHandler
    .type   Arch_SyscallHandler, @function
Arch_SyscallHandler:
    /* SYSCALL (trap class 6) does not disable IE (unlike interrupts).
     * A timer interrupt firing anywhere in this handler between svlcx and
     * rslcx+rfe would preempt the context-switch logic, overwrite 'current',
     * and corrupt a2 (caller-saved = not restored by rfe).
     * Disable interrupts for the entire handler; rslcx+rfe restores IE from
     * the PIE bit in PCXI, re-enabling them on return. */
    disable

    /* Save lower context; PCXI → lower_svlcx (UL=0) → upper_csa (UL=1) */
    svlcx

    jeq  %d15, 0, .L_yield
    jeq  %d15, 1, .L_softirq
    /* Unknown syscall: restore and return */
    rslcx
    rfe

/* -----------------------------------------------------------------------
 * Voluntary yield (TIN = 0)
 * ----------------------------------------------------------------------- */
.L_yield:
    /* Read PCXI (= lower_svlcx link) and store in current->stackpointer */
    mfcr   %d4, TRICORE_CSFR_PCXI
    movh.a %a3, hi:current
    ld.a   %a4, [%a3] lo:current    /* a4 = current task pointer */
    st.w   [%a4], %d4               /* task->stackpointer = PCXI */

    call  CoreSwitchPending         /* a2 = new task or NULL */

    jz.a  %a2, .L_sc_restore

    /* Switch to new task */
    movh.a %a3, hi:current
    st.a   [%a3] lo:current, %a2   /* current = new task */
    ld.w   %d4, [%a2]              /* d4 = new task->stackpointer (CSA link) */
    dsync
    mtcr   TRICORE_CSFR_PCXI, %d4
    isync
    rslcx
    rfe

.L_sc_restore:
    /* No switch: reload current task's PCXI and return to it */
    movh.a %a3, hi:current
    ld.a   %a4, [%a3] lo:current
    ld.w   %d4, [%a4]
    dsync
    mtcr   TRICORE_CSFR_PCXI, %d4
    isync
    rslcx
    rfe

/* -----------------------------------------------------------------------
 * Software IRQ (TIN = 1)
 * ----------------------------------------------------------------------- */
.L_softirq:
    /* Save context, dispatch through IrqDispatch, then optionally switch */
    mfcr   %d4, TRICORE_CSFR_PCXI
    movh.a %a3, hi:current
    ld.a   %a4, [%a3] lo:current
    st.w   [%a4], %d4

    mov    %d4, ARCH_TRICORE_SW_IRQ_PRIORITY
    call   IrqDispatch              /* IrqDispatch(d4 = sw_irq_priority) */
    call   CoreSwitchPending        /* a2 = new task or NULL */

    jz.a   %a2, .L_sc_restore      /* reuse the no-switch restore path */

    movh.a %a3, hi:current
    st.a   [%a3] lo:current, %a2
    ld.w   %d4, [%a2]
    dsync
    mtcr   TRICORE_CSFR_PCXI, %d4
    isync
    rslcx
    rfe

/* ===========================================================================
 * Arch_CommonIsrEntry — common entry for all interrupt priorities
 *
 * Called via J (not CALL) from the interrupt vector after BISR has:
 *   - Saved lower context (PCXI → lower_bisr_csa, UL=0)
 *   - Set ICR.CCPN = interrupt priority
 *   - Loaded A10 from ISP (interrupt stack)
 *
 * After all balanced CALL/RET pairs in IrqDispatch and CoreSwitchPending,
 * PCXI is back to lower_bisr_csa.
 *
 * Return values from C functions follow EABI:
 *   CoreSwitchPending returns TaskControBlock* in A2.
 * =========================================================================== */
    .global Arch_CommonIsrEntry
    .type   Arch_CommonIsrEntry, @function
Arch_CommonIsrEntry:
    /* Extract CCPN (bits 7:0) from ICR into %d4 for IrqDispatch argument */
    mfcr   %d4, TRICORE_CSFR_ICR
    extr.u %d4, %d4, 0, 8       /* d4 = CCPN = irq_index */
    call   IrqDispatch

    call   CoreSwitchPending     /* a2 = new task or NULL */
    jz.a   %a2, .L_isr_no_switch

    /* Forced context switch at outermost ISR exit */
    mfcr   %d4, TRICORE_CSFR_PCXI       /* d4 = lower_bisr link */
    movh.a %a3, hi:current
    ld.a   %a4, [%a3] lo:current        /* a4 = old current task */
    st.w   [%a4], %d4                   /* old->stackpointer = PCXI */
    st.a   [%a3] lo:current, %a2        /* current = new task */
    ld.w   %d4, [%a2]                   /* d4 = new task->stackpointer */
    dsync
    mtcr   TRICORE_CSFR_PCXI, %d4
    isync
    rslcx
    rfe

.L_isr_no_switch:
    rslcx
    rfe

/* ===========================================================================
 * Arch_StartFirstTask — start the first task from supervisor thread mode
 *
 * Loads the first task's CSA link into PCXI and performs RSLCX + RFE.
 * RFE semantics: PC = current A11 (restored by RSLCX) before upper context
 * is written, so the task entry point from the lower CSA is used as PC.
 * =========================================================================== */
    .global Arch_StartFirstTask
    .type   Arch_StartFirstTask, @function
Arch_StartFirstTask:
    movh.a %a2, hi:current
    ld.a   %a2, [%a2] lo:current    /* a2 = current task pointer */
    ld.w   %d0, [%a2]               /* d0 = task->stackpointer (CSA link to lower CSA) */

    dsync
    mtcr   TRICORE_CSFR_PCXI, %d0
    isync

    /* Clear PSW.CDE (bit 7) before rfe.
     *
     * The C call chain (startup → main → CoreStart → ArchStartKernel →
     * Arch_StartFirstTask) may have accumulated a non-zero CDC value.
     * The compiler-generated CALL to this function re-sets CDE=1, so
     * ArchStartKernel's earlier CDE-clear is undone.  If CDE=1 and
     * CDC != 0 and CDC != 0x7F, helper_rfe raises TIN3_NEST.
     *
     * Clearing CDE here makes the NEST condition (!cdc_zero && CDE)
     * false unconditionally, regardless of CDC. */
    mfcr   %d0, TRICORE_CSFR_PSW
    mov    %d1, 0x80                 /* CDE bit mask */
    andn   %d0, %d0, %d1            /* PSW &= ~0x80 */
    dsync
    mtcr   TRICORE_CSFR_PSW, %d0
    isync

    /* RSLCX: restores lower context, A11 = entry_point, PCXI → upper_csa */
    rslcx

    /* RFE: PC = A11 = entry_point, restores upper context (PSW, A10, ...) */
    rfe

#endif /* CONFIG_ARCH_TRICORE */
