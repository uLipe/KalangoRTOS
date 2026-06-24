/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 cooperative context switch — arch/tricore/ctx_switch.S
 *
 * void ul_arch_ctx_switch(ul_arch_ctx_t *from, const ul_arch_ctx_t *to)
 *   a4 = from  (ul_arch_ctx_t *) — {sp, ra} saved here
 *   a5 = to    (const ul_arch_ctx_t *) — {sp, ra} loaded from here
 *
 * ul_arch_ctx_t layout:
 *   [+0] sp  — A10 (stack pointer) at suspension point
 *   [+4] ra  — A11 (return address / entry) to jump to on resume
 *
 * Upper context frame layout pushed by CALL (TC1.6.1 arch manual):
 *   [0]  PCXI  [1]  PSW  [2]  A10  [3]  A11
 *   [4..11] D8-D15   [12..15] A12-A15
 *
 * PCXI link-word format (TC1.6.1 §3.2):
 *   [29:22] PCPN  [21] PIE  [20] UL  [19:16] PCXS  [15:0] PCXO
 *   Physical frame addr = PCXS << 28 | PCXO << 6
 *
 * Algorithm:
 *   1. Read PCXI — points to the CALL frame pushed entering this function.
 *   2. Decode PCXI to the frame's physical address.
 *   3. Load A10 (SP) and A11 (RA) from frame words [2] and [3].
 *   4. Save them to from->{sp, ra}.
 *   5. Free the CALL frame back to FCX, eliminating the per-switch leak:
 *        frame[0] = old FCX; FCX = this frame (PCXI with UL bit cleared).
 *      A dsync ensures the store to frame[0] is visible before FCX is
 *      updated, followed by isync to serialise the MTCR before the JI.
 *   6. Load to->{sp, ra}.
 *   7. Set A10 = to->sp, A11 = to->ra, JI A11.
 *
 * Note on PCXI and PSW.CDC:
 *   We intentionally skip updating PCXI to the frame's prior-chain link.
 *   Because this kernel uses JI (not RFE), the PCXI chain is never
 *   followed by hardware after a switch; updating it would be wasted work.
 *   For the same reason, PSW.CDC is not modified — the NEST trap fires
 *   only on RFE with CDC.COUNT != 0, which we never execute.
 */

    .text
    .global ul_arch_ctx_switch
    .type   ul_arch_ctx_switch, @function

ul_arch_ctx_switch:
    /* Step 1: capture the CALL frame link (PCXI, UL=1) */
    mfcr    %d0, 0xFE00

    /* Step 2: decode CALL frame physical address = PCXS<<28 | PCXO<<6 */
    extr.u  %d2, %d0, 16, 4     /* d2 = PCXS (bits[19:16])         */
    sh      %d2, %d2, 28         /* d2 = PCXS << 28                 */
    extr.u  %d3, %d0, 0, 16     /* d3 = PCXO (bits[15:0])          */
    sh      %d3, %d3, 6          /* d3 = PCXO << 6                  */
    or      %d2, %d2, %d3        /* d2 = frame physical address     */
    mov.a   %a6, %d2             /* a6 → CALL frame                 */

    /* Step 3 + 4: read A10/A11 from frame and save to "from" */
    ld.w    %d5, [%a6] 8         /* d5 = frame[2] = A10 (SP)        */
    ld.w    %d6, [%a6] 12        /* d6 = frame[3] = A11 (RA)        */
    st.w    [%a4] 0, %d5         /* from->sp = A10                  */
    st.w    [%a4] 4, %d6         /* from->ra = A11                  */

    /*
     * Step 5: free the CALL frame back to FCX.
     *
     * Protocol: frame[0] = old FCX; FCX = link to this frame.
     * The link for FCX uses the same PCXS/PCXO encoding as PCXI but
     * without metadata bits [31:20].  UL (bit 20) is the only metadata
     * bit set in a CALL-pushed PCXI, so clearing bit 20 gives a valid
     * FCX link:
     *   movh %d7, 16   →  d7 = 0x00100000 (UL bit mask)
     *   andn %d0, %d0, %d7  →  d0 without UL bit
     *
     * dsync: ensures the store to frame[0] completes before FCX changes.
     * isync: serialises the MTCR write before any subsequent instruction
     *        that will read FCX (e.g. the next CALL in the new thread).
     *
     * NOTE (QEMU AURIX): the Linumiz QEMU fork does not propagate
     * mtcr 0xFE38 (FCX) writes to its internal CSA allocator, so this
     * frame-freeing code has no effect in the emulator.  On silicon the
     * mtcr is effective and there is no net frame consumption per switch.
     * The stress-test Makefile uses a 256-frame pool (via --defsym) to
     * accommodate the per-switch leak in QEMU without requiring any
     * change to this production code path.
     */
    mfcr    %d7, 0xFE38          /* d7 = current FCX                */
    st.w    [%a6] 0, %d7         /* frame[0] = old FCX              */
    dsync
    movh    %d7, 16              /* d7 = 0x00100000 (UL bit)        */
    andn    %d0, %d0, %d7        /* d0 = PCXI without UL bit        */
    mtcr    0xFE38, %d0          /* FCX = freed frame (new head)    */
    isync

    /* Step 6 + 7: load new task context and jump */
    ld.w    %d0, [%a5] 0         /* d0 = to->sp                     */
    ld.w    %d1, [%a5] 4         /* d1 = to->ra                     */
    mov.a   %a10, %d0            /* A10 = new SP                    */
    mov.a   %a11, %d1            /* A11 = new RA / entry point      */
    ji      %a11

    .size ul_arch_ctx_switch, . - ul_arch_ctx_switch
