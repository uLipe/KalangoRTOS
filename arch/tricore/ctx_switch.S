/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 context switch and thread trampoline
 * arch/tricore/ctx_switch.S
 *
 * -------------------------------------------------------------------------
 * ul_arch_ctx_switch — cooperative context switch
 * -------------------------------------------------------------------------
 *
 * void ul_arch_ctx_switch(ul_arch_ctx_t *from, const ul_arch_ctx_t *to)
 *   a4 = from  (ul_arch_ctx_t *) — {pcxi} written here on suspension
 *   a5 = to    (const ul_arch_ctx_t *) — {pcxi} loaded from here on resume
 *
 * On TriCore every CALL saves the upper context to a new CSA frame (UL=1)
 * and updates PCXI.  SVLCX saves the lower context to another CSA frame
 * (UL=0) on top of that.  RSLCX+RFE free both frames on the way out.
 *
 * Suspension of "from":
 *   CALL → pushes upper-context frame (UL=1), PCXI = link to it
 *   SVLCX → pushes lower-context frame (UL=0), PCXI = link to it
 *   chain: lower(UL=0) → upper(UL=1) → previous PCXI
 *   Store PCXI → from->pcxi
 *
 * Resumption of "to":
 *   Load to->pcxi → PCXI
 *   RSLCX → restores lower context, frees lower CSA frame, PCXI → upper
 *   RFE   → restores upper context (PSW, A10, A11, D8-D11, A12-A15, D12-D15),
 *            frees upper CSA frame, jumps to A11
 *
 * NOTE: QEMU Linumiz CSA layout differs from TC1.6.1 spec:
 *   Upper (CALL/RFE): PCXI PSW A10 A11 D8-D11 A12-A15 D12-D15
 *   Lower (SVLCX/RSLCX): PCXI A11 A2 A3 D0-D3 A4-A7 D4-D7
 *
 * For a freshly fabricated thread "to", ul_arch_ctx_init() builds:
 *   lower_csa[0]  = upper_link | UL_FLAG  (→ upper frame)
 *   lower_csa[1]  = _ul_thread_trampoline (A11 after RSLCX; RFE uses
 *                                          this for PC before restoring upper)
 *   lower_csa[8]  = arg pointer           (A4 after RSLCX, ABI pointer arg)
 *   upper_csa[1]  = PSW
 *   upper_csa[2]  = stack_top             (A10)
 *   upper_csa[3]  = _ul_thread_trampoline (A11 register after RFE)
 *   upper_csa[10] = entry function        (A14 after RFE, trampoline calls it)
 *
 * -------------------------------------------------------------------------
 * _ul_thread_trampoline — first instruction of every new thread
 * -------------------------------------------------------------------------
 *
 * After RSLCX + RFE:
 *   A10 = stack_top      (upper_csa[2] via RFE)
 *   A14 = entry function (upper_csa[10] via RFE)
 *   A4  = arg pointer    (lower_csa[8] via RSLCX)
 *
 * TriCore ABI: pointer args in A4-A7.  A4 is ready; just call via A14.
 */

    .extern ul_arch_cpu_halt

/* ---------------------------------------------------------------------------
 * ul_arch_ctx_switch
 * ------------------------------------------------------------------------ */

    .text
    .global ul_arch_ctx_switch
    .type   ul_arch_ctx_switch, @function

ul_arch_ctx_switch:
    svlcx                        /* save lower context → new CSA frame     */
    mfcr    %d0, 0xFE00          /* d0 = PCXI: head of "from" CSA chain    */
    st.w    [%a4] 0, %d0         /* from->pcxi = PCXI                      */
    ld.w    %d0, [%a5] 0         /* d0 = to->pcxi                          */
    dsync                        /* ensure CSA writes visible before MTCR  */
    mtcr    0xFE00, %d0          /* PCXI = to->pcxi                        */
    isync
    /*
     * Clear CDC (PSW[6:0]) before RSLCX+RFE.
     *
     * QEMU Linumiz initialises PSW with CDE=1.  Every CALL accumulates
     * CDC > 0.  RFE traps NEST when CDC != 0 AND CDE = 1, because we
     * are calling RFE from a CALL chain rather than from an ISR.
     * Zeroing CDC satisfies cdc_zero() and bypasses the NEST check.
     * After RFE, PSW is restored from the upper CSA frame (fabricated
     * or previously saved), so this MTCR only affects the RFE check.
     */
    mfcr    %d1, 0xFE04          /* d1 = current PSW                       */
    mov     %d2, 0x7f
    andn    %d1, %d1, %d2        /* d1 &= ~0x7F  (clear CDC bits [6:0])    */
    mtcr    0xFE04, %d1          /* update PSW: CDC = 0                    */
    isync
    rslcx                        /* restore "to" lower context; free frame */
    rfe                          /* restore "to" upper context; free frame;
                                  * jump to A11 (entry or return address)  */

    .size ul_arch_ctx_switch, . - ul_arch_ctx_switch

/* ---------------------------------------------------------------------------
 * _ul_thread_trampoline
 * ------------------------------------------------------------------------ */

    .global _ul_thread_trampoline
    .type   _ul_thread_trampoline, @function

_ul_thread_trampoline:
    calli   %a14                 /* entry(arg): A14=entry fn, A4=arg ptr   */
    call    ul_arch_cpu_halt
    j       .

    .size _ul_thread_trampoline, . - _ul_thread_trampoline
