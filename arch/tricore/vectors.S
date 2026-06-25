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

    .section .int_table, "ax"
    .align  8   /* 256-byte aligned BIV base */

    .global _ul_int_table
_ul_int_table:

/* SRPN=0 — priority 0 is disabled; this slot is never dispatched. */
    rfe
    .align  5		/* pad to next 32-byte boundary */

/* SRPN=1 — kernel tick one-shot ISR (BIV + 1*32 = _ul_int_table + 0x20) */
    svlcx
    call    _arch_tick_isr_handler
    disable
    rslcx
    rfe
    .align  5		/* pad to next 32-byte boundary */
