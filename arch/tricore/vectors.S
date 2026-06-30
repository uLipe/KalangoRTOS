/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 trap and interrupt vector tables — arch/tricore/vectors.S
 * Full specification: docs/arch_api_spec.md §8
 *
 * Trap classes 0–7, each 8 words (32 bytes) apart within a 256-byte-aligned
 * base address (BTV).  The hardware indexes: BTV + (class * 0x20) + (TIN * 4)
 * for class 6 (SYSCALL); all others jump through a common handler.
 *
 * Interrupt table: 256-byte-aligned BIV; each entry is a 4-byte jump.
 * We register a single generic ISR dispatcher at vector 0 here; individual
 * SRPN vectors are programmed at runtime by ul_arch_irq_src_configure().
 */

    .extern ul_kernel_trap_fault
    .extern ul_arch_syscall_entry
    .extern ul_kernel_irq_dispatch

/* ---------------------------------------------------------------------------
 * Trap vector table
 * ------------------------------------------------------------------------ */

    .section .trap_table, "ax"
    .align  8   /* 256 bytes */

/* Class 0 — MMU / MPU Data Access Trap */
    .global _trap_class0
_trap_class0:
    mov     %d4, 0
    mov     %d5, %d15
    call    ul_kernel_trap_fault
    j       .

/* Class 1 — Internal Protection Trap */
    .align  5
    .global _trap_class1
_trap_class1:
    mov     %d4, 1
    mov     %d5, %d15
    call    ul_kernel_trap_fault
    j       .

/* Class 2 — Instruction Error Trap */
    .align  5
    .global _trap_class2
_trap_class2:
    mov     %d4, 2
    mov     %d5, %d15
    call    ul_kernel_trap_fault
    j       .

/* Class 3 — Context Management Trap (FCU/CDU) */
    .align  5
    .global _trap_class3
_trap_class3:
    mov     %d4, 3
    mov     %d5, %d15
    call    ul_kernel_trap_fault
    j       .

/* Class 4 — System Bus and Peripheral Errors */
    .align  5
    .global _trap_class4
_trap_class4:
    mov     %d4, 4
    mov     %d5, %d15
    call    ul_kernel_trap_fault
    j       .

/* Class 5 — Assertion Traps */
    .align  5
    .global _trap_class5
_trap_class5:
    mov     %d4, 5
    mov     %d5, %d15
    call    ul_kernel_trap_fault
    j       .

/* Class 6 — SYSCALL */
    .align  5
    .global _trap_class6
_trap_class6:
    call    ul_arch_syscall_entry
    rfe

/* Class 7 — NMI */
    .align  5
    .global _trap_class7
_trap_class7:
    mov     %d4, 7
    mov     %d5, %d15
    call    ul_kernel_trap_fault
    j       .

/* ---------------------------------------------------------------------------
 * Interrupt vector table — 32-byte slots, BIV-relative (VSS=0)
 *
 * On interrupt entry the hardware:
 *   1. Saves the Upper Context (PSW, A10, A11, D8–D15) into a fresh CSA frame
 *   2. Sets A10 = ISP, IS=1, IE=0, CCPN=SRPN
 *   3. Jumps to BIV + SRPN * 32
 *
 * Each ISR entry must save the Lower Context (SVLCX), call the C handler,
 * restore the Lower Context (RSLCX), and return (RFE).
 * DISABLE before RSLCX prevents a higher-priority interrupt from nesting
 * during the critical restore sequence.
 * ------------------------------------------------------------------------ */

    .extern _arch_tick_isr_handler
    .extern _arch_generic_isr_handler

    .section .int_table, "ax"
    .align  8   /* 256-byte aligned BIV base */

    .global _ul_int_table
_ul_int_table:

/* SRPN=0 — priority 0 is disabled; this slot is never dispatched. */
    rfe
    .align  5		/* pad to next 32-byte boundary */

/* SRPN=1 — kernel tick one-shot ISR (BIV + 1*32 = _ul_int_table + 0x20) */
    j       _arch_tick_preempt_isr    /* preemption-aware trampoline */
    .align  5		/* pad to next 32-byte boundary */

/*
 * SRPN=2..255 — generic userspace IRQ dispatcher.
 *
 * On entry the hardware has already:
 *   - saved the Upper Context into a fresh CSA frame
 *   - set ICR.CCPN = SRPN of the fired interrupt
 *   - cleared ICR.IE
 *   - switched A10 to ISP (if PSW.IS was 0)
 *
 * CSA/CSFR accesses (SVLCX, CALL, RFE, MFCR/MTCR) bypass DPR protection,
 * so the stub is safe even when running in user PRS (PRS=1).
 * PRS elevation to kernel (PRS=0) is performed inside _arch_generic_isr_handler
 * before any data stack access.
 */
    .rept   254
    svlcx
    call    _arch_generic_isr_handler
    disable
    rslcx
    rfe
    .align  5
    .endr

/* ---------------------------------------------------------------------------
 * _arch_tick_preempt_isr — tick ISR with preemptive context switch support
 *
 * The SRPN=1 vector slot jumps here (no CALL, so no extra CSA frame).
 * On entry the hardware has already:
 *   - saved the Upper Context of the interrupted thread -> U_hw
 *   - set ICR.IE=0, ICR.CCPN=1
 *   - switched A10 to ISP (if PSW.IS was 0)
 *
 * Flow:
 *   svlcx                  -> saves Lower Context -> L_sv; PCXI = L_sv->U_hw
 *   call _arch_tick_isr_handler  -> executes timer + sched_tick C logic
 *                                   may set g_preempt_old/new_ctx
 *   ret from call           -> PCXI = L_sv->U_hw  (CALL frame freed by ret)
 *
 *   if g_preempt_new_ctx != NULL:
 *     save PCXI -> g_preempt_old_ctx->pcxi  (records L_sv->U_hw for later resume)
 *     PCXI <- g_preempt_new_ctx->pcxi        (switch to next thread's CSA chain)
 *     clear g_preempt_new_ctx
 *     clear CDC (prevents NEST trap on RFE from CALL chain, see ctx_switch.S)
 *     rslcx + rfe  -> restore next thread context
 *   else:
 *     rslcx + rfe  -> restore interrupted thread
 * ------------------------------------------------------------------------ */

    .section .text, "ax"
    .extern _arch_tick_isr_handler
    .extern g_preempt_old_ctx
    .extern g_preempt_new_ctx

    .global _arch_tick_preempt_isr
    .type   _arch_tick_preempt_isr, @function

_arch_tick_preempt_isr:
    svlcx
    call    _arch_tick_isr_handler
    /* PCXI = L_sv -> U_hw at this point */

    /* Load g_preempt_new_ctx into a14 */
    movh.a  %a14, hi:g_preempt_new_ctx
    lea     %a14, [%a14] lo:g_preempt_new_ctx
    ld.a    %a14, [%a14]          /* a14 = g_preempt_new_ctx (ctx* or NULL) */
    jz.a    %a14, .Ltick_no_preempt

    /* Preemptive switch requested */

    /* a14 = new_ctx->pcxi source */
    movh.a  %a15, hi:g_preempt_old_ctx
    lea     %a15, [%a15] lo:g_preempt_old_ctx
    ld.a    %a15, [%a15]          /* a15 = g_preempt_old_ctx (ctx* of outgoing) */

    /* Consume the flag: clear g_preempt_new_ctx */
    mov.a   %a12, 0
    movh.a  %a13, hi:g_preempt_new_ctx
    lea     %a13, [%a13] lo:g_preempt_new_ctx
    st.a    [%a13], %a12

    /* Save current PCXI chain (L_sv->U_hw) to outgoing thread's ctx */
    mfcr    %d0, 0xFE00
    st.w    [%a15] 0, %d0

    /* Install incoming thread's PCXI */
    ld.w    %d0, [%a14] 0
    dsync
    mtcr    0xFE00, %d0
    isync

    /* Clear CDC before RFE to prevent NEST trap (see ctx_switch.S) */
    mfcr    %d1, 0xFE04
    mov     %d2, 0x7f
    andn    %d1, %d1, %d2
    mtcr    0xFE04, %d1
    isync

    disable
    rslcx
    rfe

.Ltick_no_preempt:
    disable
    rslcx
    rfe

    .size _arch_tick_preempt_isr, . - _arch_tick_preempt_isr
