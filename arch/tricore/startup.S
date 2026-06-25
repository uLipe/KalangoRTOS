/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 kernel startup — arch/tricore/startup.S
 *
 * Calling sequence (docs/arch_api_spec.md §11.3):
 *   1. Set A10 = _ul_kernel_stack_top
 *   2. Build CSA free list from _ul_csa_pool_start / _ul_csa_pool_end
 *   3. ul_board_init()  — optional chip setup, weak no-op if absent
 *   4. Copy _ul_kernel_data (flash LMA → SRAM VMA)
 *   5. Zero _ul_kernel_data bss range
 *   6. Set small-data address registers (A0, A1, A8, A9)
 *   7. ul_arch_init(&boot_info)
 *   8. ul_kernel_main(&boot_info)  — does not return
 */

    .extern _ul_kernel_stack_top
    .extern _ul_csa_pool_start
    .extern _ul_csa_pool_end
    .extern __ul_kernel_data_load
    .extern _ul_kernel_data_start
    .extern _ul_kernel_data_end
    .extern _ul_kernel_bss_start
    .extern _ul_kernel_bss_end
    .extern _small_data_
    .extern _small_data2_
    .extern _small_data3_
    .extern _small_data4_
    .extern ul_board_init
    .extern ul_arch_init
    .extern ul_kernel_main

    .section .text._start, "ax"
    .global _start
    .type   _start, @function
_start:
    /* 1. Supervisor stack */
    movh.a  %a10, hi:_ul_kernel_stack_top
    lea     %a10, [%a10] lo:_ul_kernel_stack_top

    /* 2. CSA free list construction.
     *
     * CSA_TO_ADDR / ADDR_TO_CSA mapping for SRAM alias 0x7xxx_xxxx:
     *   link = 0x00070000 | extr.u(addr, 6, 16)
     *
     * We iterate pool_base..pool_end in 64-byte strides.
     */
    movh.a  %a2, hi:_ul_csa_pool_start
    lea     %a2, [%a2] lo:_ul_csa_pool_start
    movh.a  %a3, hi:_ul_csa_pool_end
    lea     %a3, [%a3] lo:_ul_csa_pool_end

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

    /* 3. Board-level chip setup (weak no-op if not provided). */
    call    ul_board_init

    /* 4. Copy .kernel_data from flash (LMA) to SRAM (VMA). */
    movh.a  %a5, hi:__ul_kernel_data_load
    lea     %a5, [%a5] lo:__ul_kernel_data_load
    movh.a  %a6, hi:_ul_kernel_data_start
    lea     %a6, [%a6] lo:_ul_kernel_data_start
    movh.a  %a7, hi:_ul_kernel_data_end
    lea     %a7, [%a7] lo:_ul_kernel_data_end

.L_data_copy:
    mov.d   %d0, %a6
    mov.d   %d1, %a7
    jge     %d0, %d1, .L_data_done
    ld.w    %d0, [%a5]
    st.w    [%a6], %d0
    lea     %a5, [%a5] 4
    lea     %a6, [%a6] 4
    j       .L_data_copy
.L_data_done:

    /* 5. Zero BSS: clear [_ul_kernel_bss_start .. _ul_kernel_bss_end). */
    movh.a  %a5, hi:_ul_kernel_bss_start
    lea     %a5, [%a5] lo:_ul_kernel_bss_start
    movh.a  %a6, hi:_ul_kernel_bss_end
    lea     %a6, [%a6] lo:_ul_kernel_bss_end
    mov     %d0, 0

.L_bss_zero:
    mov.d   %d1, %a5
    mov.d   %d2, %a6
    jge     %d1, %d2, .L_bss_done
    st.w    [%a5], %d0
    lea     %a5, [%a5] 4
    j       .L_bss_zero

.L_bss_done:

    /* 6. Small-data address registers. */
    movh.a  %a0, hi:_small_data_
    lea     %a0, [%a0] lo:_small_data_
    movh.a  %a1, hi:_small_data2_
    lea     %a1, [%a1] lo:_small_data2_
    movh.a  %a8, hi:_small_data3_
    lea     %a8, [%a8] lo:_small_data3_
    movh.a  %a9, hi:_small_data4_
    lea     %a9, [%a9] lo:_small_data4_

    /* 7. Architecture init (fills boot_info, programs BTV/BIV/ISP). */
    movh.a  %a4, 0               /* pass &boot_info; TODO: reserve storage */
    call    ul_arch_init

    /* 8. Kernel main — does not return. */
    call    ul_kernel_main

.L_hang:
    j       .L_hang
    .size   _start, . - _start
