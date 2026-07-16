/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 context switch and thread trampoline
 * arch/tricore/ctx_switch.S
 *
 * -------------------------------------------------------------------------
 * ulmk_arch_ctx_switch — cooperative context switch
 * -------------------------------------------------------------------------
 *
 * void ulmk_arch_ctx_switch(ulmk_arch_ctx_t *from, const ulmk_arch_ctx_t *to)
 *   a4 = from  (ulmk_arch_ctx_t *) — {pcxi} written here on suspension
 *   a5 = to    (const ulmk_arch_ctx_t *) — {pcxi} loaded from here on resume
 *
 * On TriCore every CALL saves the upper context to a new CSA frame (UL=1)
 * and updates PCXI.  SVLCX saves the lower context to another CSA frame
 * (UL=0) on top of that.  RSLCX+RFE free both frames on the way out.
 *
 * Canonical hot path (book / Zephyr-style — no C mid-switch):
 *   disable → svlcx → store PCXI → load PCXI → dsync → mtcr PCXI →
 *   isync → (cdc_zero) → rslcx → rfe
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
 * TC1.6.1 CSA layout:
 *   Upper (CALL/RFE):    PCXI PSW A10 A11 D8-D11 A12-A15 D12-D15
 *   Lower (SVLCX/RSLCX): PCXI A11 A2 A3 D0-D3 A4-A7 D4-D7
 *
 * For a freshly fabricated thread "to", ulmk_arch_ctx_init() builds:
 *   lower_csa[0]  = upper_link | UL_FLAG | PIE
 *   lower_csa[1]  = ulmk_user_thread_entry (A11 after RSLCX)
 *   lower_csa[8]  = thread entry fn       (A4 — ABI arg0)
 *   lower_csa[9]  = thread arg            (A5 — ABI arg1)
 *   upper_csa[1]  = PSW
 *   upper_csa[2]  = stack_top             (A10)
 *   upper_csa[3]  = ulmk_user_thread_entry (RFE PC; must be in CPR 1)
 *
 * PC lands directly in .user_runtime so PRS 1 threads never fetch CPR 0.
 */

    .extern ulmk_arch_cpu_halt

/* ---------------------------------------------------------------------------
 * ulmk_arch_ctx_switch
 * ------------------------------------------------------------------------ */

    .text
    .global ulmk_arch_ctx_switch
    .type   ulmk_arch_ctx_switch, @function

ulmk_arch_ctx_switch:
    /*
     * Mask all maskable interrupts for the duration of the switch.
     *
     * Without this, a timer interrupt arriving after "mtcr PCXI, to->pcxi"
     * but before "rslcx" would prepend its own ISR CSA frames to the
     * destination thread's chain, corrupting it when that thread next runs.
     * RFE at the end restores ICR.IE from the upper context's PIE bit,
     * so the new thread starts with interrupts in the correct state.
     */
    disable
    svlcx                        /* save lower context → new CSA frame     */
    mfcr    %d0, 0xFE00          /* d0 = PCXI: head of "from" CSA chain    */
    st.w    [%a4] 0, %d0         /* from->pcxi = PCXI                      */
    ld.w    %d0, [%a5] 0         /* d0 = to->pcxi                          */
    dsync                        /* CSA stores visible before MTCR PCXI    */
    mtcr    0xFE00, %d0          /* PCXI = to->pcxi                        */
    isync
    /*
     * Clear CDC (PSW[6:0]) before RSLCX+RFE.
     *
     * When CDE=1, every CALL increments CDC.  RFE from a CALL chain
     * (not from an ISR) traps NEST when CDC != 0 and CDE = 1.
     * Zeroing CDC satisfies cdc_zero() and bypasses the NEST check.
     * After RFE, PSW is restored from the upper CSA frame so this
     * MTCR only affects the one upcoming RFE.
     */
    mfcr    %d1, 0xFE04          /* d1 = current PSW                       */
    mov     %d2, 0x7f
    andn    %d1, %d1, %d2        /* d1 &= ~0x7F  (clear CDC bits [6:0])    */
    mtcr    0xFE04, %d1          /* update PSW: CDC = 0                    */
    isync
    rslcx                        /* restore "to" lower context; free frame */
    rfe                          /* restore "to" upper context; free frame;
                                  * jump to A11 (entry or return address)  */

    .size ulmk_arch_ctx_switch, . - ulmk_arch_ctx_switch
