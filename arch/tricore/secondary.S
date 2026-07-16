/* SPDX-License-Identifier: MIT */
/*
 * CPU1 cold entry — arch/tricore/secondary.S
 *
 * Reset leaves CPU1 halted.  CPU0 programs PC to _ulmk_cpu1_start and clears
 * DBGSR.HALT.  This prologue builds a private CSA free list in DSPR1
 * (segment 0x6), loads park SP + small-data anchors, then jumps to
 * ulmk_arch_smp_park() which waits for release and calls the kernel entry.
 */

    .extern _ulmk_park_stack_cpu1_top
    .extern _ulmk_csa_pool_cpu1_start
    .extern _ulmk_csa_pool_cpu1_end
    .extern _small_data_
    .extern _small_data2_
    .extern _small_data3_
    .extern _small_data4_
    .extern ulmk_arch_smp_park

    .section .text._ulmk_cpu1_start, "ax"
    .global _ulmk_cpu1_start
    .type   _ulmk_cpu1_start, @function
	.set WDTCPU1_CON0, 0xF003610C
	.set WDTCPU1_CON1, 0xF0036110

_ulmk_cpu1_start:
    disable

    movh.a  %a10, hi:_ulmk_park_stack_cpu1_top
    lea     %a10, [%a10] lo:_ulmk_park_stack_cpu1_top

    /*
     * Disarm WDTCPU1 before CSA construction.  OpenOCD reset-end may
     * already have set DR, but button/PORST and flash paths without a
     * sticky DAP poke leave the default timeout armed — CSA init is
     * long enough to expire it.  Own-core EndInit only (never from CPU0).
     */
    movh.a  %a2, hi:WDTCPU1_CON0
    lea     %a2, [%a2] lo:WDTCPU1_CON0
    ld.w    %d0, [%a2]0
    extr.u  %d8, %d0, 2, 14		/* PW */
    mov     %d1, 0x3F
    xor     %d8, %d1
    extr.u  %d9, %d0, 16, 16		/* REL */
    extr.u  %d2, %d0, 1, 1		/* LCK? */
    jz      %d2, .L_wdt1_cle2
    sh      %d0, %d8, 2
    mov     %d1, 1
    or      %d0, %d1
    sh      %d1, %d9, 16
    or      %d0, %d1
    st.w    [%a2]0, %d0
.L_wdt1_cle2:
    sh      %d0, %d8, 2
    mov     %d1, 2			/* ENDINIT=0 LCK=1 */
    or      %d0, %d1
    sh      %d1, %d9, 16
    or      %d0, %d1
    st.w    [%a2]0, %d0
.L_wdt1_wait0:
    ld.w    %d0, [%a2]0
    and     %d1, %d0, 1
    jnz     %d1, .L_wdt1_wait0

    movh.a  %a3, hi:WDTCPU1_CON1
    lea     %a3, [%a3] lo:WDTCPU1_CON1
    mov     %d0, 8			/* DR=1 */
    st.w    [%a3]0, %d0

    ld.w    %d0, [%a2]0
    extr.u  %d9, %d0, 16, 16
    extr.u  %d2, %d0, 1, 1
    jz      %d2, .L_wdt1_set2
    sh      %d0, %d8, 2
    mov     %d1, 1
    or      %d0, %d1
    sh      %d1, %d9, 16
    or      %d0, %d1
    st.w    [%a2]0, %d0
.L_wdt1_set2:
    sh      %d0, %d8, 2
    mov     %d1, 3			/* ENDINIT=1 LCK=1 */
    or      %d0, %d1
    sh      %d1, %d9, 16
    or      %d0, %d1
    st.w    [%a2]0, %d0
.L_wdt1_wait1:
    ld.w    %d0, [%a2]0
    and     %d1, %d0, 1
    jz      %d1, .L_wdt1_wait1

    /*
     * CSA free list for DSPR1 @ 0x6xxx_xxxx:
     *   link = 0x00060000 | extr.u(addr, 6, 16)
     */
    movh.a  %a2, hi:_ulmk_csa_pool_cpu1_start
    lea     %a2, [%a2] lo:_ulmk_csa_pool_cpu1_start
    movh.a  %a3, hi:_ulmk_csa_pool_cpu1_end
    lea     %a3, [%a3] lo:_ulmk_csa_pool_cpu1_end

    lea     %a4, [%a2] 0

.L_csa1_link:
    lea     %a5, [%a4] 64
    mov.d   %d0, %a5
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 6
    or      %d0, %d1
    mov.d   %d2, %a5
    mov.d   %d3, %a3
    jge     %d2, %d3, .L_csa1_last
    st.w    [%a4], %d0
    lea     %a4, [%a5] 0
    j       .L_csa1_link

.L_csa1_last:
    mov     %d0, 0
    st.w    [%a4], %d0

    mov.d   %d0, %a2
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 6
    or      %d0, %d1
    dsync
    mtcr    0xFE38, %d0          /* FCX */
    isync

    lea     %a6, [%a4] 0
    mov.d   %d0, %a6
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 6
    or      %d0, %d1
    dsync
    mtcr    0xFE3C, %d0          /* LCX */
    isync

    movh.a  %a0, hi:_small_data_
    lea     %a0, [%a0] lo:_small_data_
    movh.a  %a1, hi:_small_data2_
    lea     %a1, [%a1] lo:_small_data2_
    movh.a  %a8, hi:_small_data3_
    lea     %a8, [%a8] lo:_small_data3_
    movh.a  %a9, hi:_small_data4_
    lea     %a9, [%a9] lo:_small_data4_

    j       ulmk_arch_smp_park

    .size   _ulmk_cpu1_start, . - _ulmk_cpu1_start
