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
 * SRPN vectors are programmed at runtime by ulmk_arch_irq_src_configure().
 */

    .extern ulmk_arch_trap_entry
    .extern ulmk_arch_syscall_entry
    .extern ulmk_kern_irq_dispatch

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
    mfcr    %d15, 0xFE04
    mov     %d14, 0x3000
    andn    %d15, %d15, %d14
    mtcr    0xFE04, %d15
    isync
    call    ulmk_arch_trap_entry
    j       .

/* Class 1 — Internal Protection Trap */
    .align  5
    .global _trap_class1
_trap_class1:
    mov     %d4, 1
    mov     %d5, %d15
    mfcr    %d15, 0xFE04
    mov     %d14, 0x3000
    andn    %d15, %d15, %d14
    mtcr    0xFE04, %d15
    isync
    call    ulmk_arch_trap_entry
    j       .

/* Class 2 — Instruction Error Trap */
    .align  5
    .global _trap_class2
_trap_class2:
    mov     %d4, 2
    mov     %d5, %d15
    mfcr    %d15, 0xFE04
    mov     %d14, 0x3000
    andn    %d15, %d15, %d14
    mtcr    0xFE04, %d15
    isync
    call    ulmk_arch_trap_entry
    j       .

/* Class 3 — Context Management Trap (FCU/CDU) */
    .align  5
    .global _trap_class3
_trap_class3:
    mov     %d4, 3
    mov     %d5, %d15
    mfcr    %d15, 0xFE04
    mov     %d14, 0x3000
    andn    %d15, %d15, %d14
    mtcr    0xFE04, %d15
    isync
    call    ulmk_arch_trap_entry
    j       .

/* Class 4 — System Bus and Peripheral Errors */
    .align  5
    .global _trap_class4
_trap_class4:
    mov     %d4, 4
    mov     %d5, %d15
    mfcr    %d15, 0xFE04
    mov     %d14, 0x3000
    andn    %d15, %d15, %d14
    mtcr    0xFE04, %d15
    isync
    call    ulmk_arch_trap_entry
    j       .

/* Class 5 — Assertion Traps */
    .align  5
    .global _trap_class5
_trap_class5:
    mov     %d4, 5
    mov     %d5, %d15
    mfcr    %d15, 0xFE04
    mov     %d14, 0x3000
    andn    %d15, %d15, %d14
    mtcr    0xFE04, %d15
    isync
    call    ulmk_arch_trap_entry
    j       .

/* Class 6 — SYSCALL */
    .align  5
    .global _trap_class6
_trap_class6:
    call    ulmk_arch_syscall_entry
    rfe

/* Class 7 — NMI */
    .align  5
    .global _trap_class7
_trap_class7:
    mov     %d4, 7
    mov     %d5, %d15
    mfcr    %d15, 0xFE04
    mov     %d14, 0x3000
    andn    %d15, %d15, %d14
    mtcr    0xFE04, %d15
    isync
    call    ulmk_arch_trap_entry
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

    .extern _arch_generic_isr_handler

    .section .int_table, "ax"
    .align  8   /* 256-byte aligned BIV base */

    .global _ulmk_int_table
_ulmk_int_table:

/* SRPN=0 — priority 0 is disabled; this slot is never dispatched. */
    rfe
    .align  5		/* pad to next 32-byte boundary */

/*
 * SRPN=1..255 — generic userspace IRQ dispatcher with preemption support.
 * Board services bind hardware IRQs (e.g. STM0) to notifications via
 * ulmk_irq_bind_hw(); the kernel dispatches by SRPN read from ICR.CCPN.
 */
    .rept   255
    j       _arch_generic_preempt_isr
    .align  5
    .endr

/* ---------------------------------------------------------------------------
 * _arch_generic_preempt_isr — generic IRQ handler with preemption support.
 *
 * Jumped to by every SRPN=2..255 vector slot.  The SRPN is read from
 * ICR.CCPN inside _arch_generic_isr_handler, which also calls
 * ulmk_kern_irq_check_preempt() to potentially set g_preempt_old/new_ctx.
 *
 * If g_preempt_new_ctx is set on return from the C handler, perform the
 * preemptive context switch identically to _arch_tick_preempt_isr.
 * ------------------------------------------------------------------------ */

    .extern _arch_generic_isr_handler

    .global _arch_generic_preempt_isr
    .type   _arch_generic_preempt_isr, @function

_arch_generic_preempt_isr:
    /*
     * PRS 0 + supervisor IO before svlcx — CSA pool lives in kernel RAM
     * and is unreachable at PRS 1 (userspace protection set).
     */
    mfcr    %d15, 0xFE04
    mov     %d14, 0x3C00
    andn    %d15, %d15, %d14
    mov     %d14, 0x800
    or      %d15, %d14
    mtcr    0xFE04, %d15
    isync
    svlcx
    call    _arch_generic_isr_handler

    movh.a  %a14, hi:g_preempt_new_ctx
    lea     %a14, [%a14] lo:g_preempt_new_ctx
    ld.a    %a14, [%a14]
    jz.a    %a14, .Lgeneric_no_preempt

    movh.a  %a15, hi:g_preempt_old_ctx
    lea     %a15, [%a15] lo:g_preempt_old_ctx
    ld.a    %a15, [%a15]

    mov.a   %a12, 0
    movh.a  %a13, hi:g_preempt_new_ctx
    lea     %a13, [%a13] lo:g_preempt_new_ctx
    st.a    [%a13], %a12

    mfcr    %d0, 0xFE00
    st.w    [%a15] 0, %d0

    ld.w    %d0, [%a14] 0
    dsync
    mtcr    0xFE00, %d0
    isync

    mfcr    %d1, 0xFE04
    mov     %d2, 0x7f
    andn    %d1, %d1, %d2
    mtcr    0xFE04, %d1
    isync

    disable
    rslcx
    rfe

.Lgeneric_no_preempt:
    disable
    rslcx
    rfe

    .size _arch_generic_preempt_isr, . - _arch_generic_preempt_isr
