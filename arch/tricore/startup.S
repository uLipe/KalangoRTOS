/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 kernel startup — arch/tricore/startup.S
 *
 * Minimal CPU-mandatory prologue only; the shared C runtime bring-up lives in
 * kernel/init/init.c (ulmk_kern_start).  Sequence:
 *   0. Disable interrupts (kernel unlocks them later)
 *   0b. Optional board early-WDT hook (weak stub; no CALL — CSA not ready)
 *   1. Set A10 = _ulmk_kernel_stack_top
 *   2. Build CSA free list (FCX/LCX) — mandatory before ANY call
 *   3. Set small-data address registers (A0, A1, A8, A9)
 *   4. Jump to ulmk_kern_start() — does not return
 *
 * Board PLL / peripheral bring-up belongs in ulmk_board_init().
 */

    .extern _ulmk_kernel_stack_top
    .extern _ulmk_csa_pool_start
    .extern _ulmk_csa_pool_end
    .extern _small_data_
    .extern _small_data2_
    .extern _small_data3_
    .extern _small_data4_
    .extern ulmk_kern_start
    .extern ulmk_board_wdt_disable_early

    .section .text._start, "ax"
    .global _start
    .type   _start, @function
_start:
    /* 0. Interrupts off until the kernel unlocks them. */
    disable

    /*
     * Board hook: disable CPU0 + Safety WDT before CSA construction.
     * Default timeout can expire during the CSA loop on standalone reset
     * (OpenOCD reset-end pokes WDT only under OCDS).  Weak stub is ji %a11.
     * No CALL (CSA not ready).
     */
    movh.a  %a11, hi:_start_after_wdt
    lea     %a11, [%a11] lo:_start_after_wdt
    movh.a  %a2, hi:ulmk_board_wdt_disable_early
    lea     %a2, [%a2] lo:ulmk_board_wdt_disable_early
    ji      %a2
    .global _start_after_wdt
_start_after_wdt:

    /* 1. Supervisor stack + clear PSW.IS
     *
     * SSW / cold reset may leave PSW.IS=1.  board_init runs before arch_init
     * programs ISP; an early class-4 with IS=1 then nests on a bad ISP and
     * cascades.  Force IS=0 so A10 is the task/kernel stack throughout bring-up.
     */
    movh.a  %a10, hi:_ulmk_kernel_stack_top
    lea     %a10, [%a10] lo:_ulmk_kernel_stack_top
    mfcr    %d0, 0xFE04
    mov     %d1, 0x200			/* PSW.IS */
    andn    %d0, %d0, %d1
    mtcr    0xFE04, %d0
    isync

    /* 2. CSA free list construction.
     *
     * CSA_TO_ADDR / ADDR_TO_CSA mapping for SRAM alias 0x7xxx_xxxx:
     *   link = 0x00070000 | extr.u(addr, 6, 16)
     *
     * We iterate pool_base..pool_end in 64-byte strides.
     */
    movh.a  %a2, hi:_ulmk_csa_pool_start
    lea     %a2, [%a2] lo:_ulmk_csa_pool_start
    movh.a  %a3, hi:_ulmk_csa_pool_end
    lea     %a3, [%a3] lo:_ulmk_csa_pool_end

    /* a4 = current frame, a5 = next frame */
    lea     %a4, [%a2] 0

.L_csa_link:
    lea     %a5, [%a4] 64
    mov.d   %d0, %a6
    mov.d   %d0, %a5
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 7
    or      %d0, %d1
    /* Check if next frame is still within pool */
    mov.d   %d2, %a5
    mov.d   %d3, %a3
    jge     %d2, %d3, .L_csa_last
    st.w    [%a4], %d0
    lea     %a4, [%a5] 0
    j       .L_csa_link

.L_csa_last:
    mov     %d0, 0
    st.w    [%a4], %d0           /* terminate chain */

    /* FCX = ADDR_TO_CSA(pool_base) */
    mov.d   %d0, %a2
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 7
    or      %d0, %d1
    dsync
    mtcr    0xFE38, %d0          /* FCX */
    isync

    /* LCX = ADDR_TO_CSA(pool_end - 64)  → triggers FCD trap before true exhaustion */
    lea     %a6, [%a4] 0
    mov.d   %d0, %a6
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 7
    or      %d0, %d1
    dsync
    mtcr    0xFE3C, %d0          /* LCX */
    isync

    /* 3. Small-data address registers (before any C access to small data). */
    movh.a  %a0, hi:_small_data_
    lea     %a0, [%a0] lo:_small_data_
    movh.a  %a1, hi:_small_data2_
    lea     %a1, [%a1] lo:_small_data2_
    movh.a  %a8, hi:_small_data3_
    lea     %a8, [%a8] lo:_small_data3_
    movh.a  %a9, hi:_small_data4_
    lea     %a9, [%a9] lo:_small_data4_

    /* 4. Common C runtime bring-up (relocates .data/.bss, calls board_init). */
    j       ulmk_kern_start

.L_hang:
    j       .L_hang
    .size   _start, . - _start
