/* SPDX-License-Identifier: MIT */
/*
 * Secondary cold entry — arch/tricore/secondary.S
 *
 * Reset leaves secondaries halted.  CPU0 programs each PC to _ulmk_cpuN_start
 * and clears DBGSR.HALT.  The prologue builds a private CSA free list in that
 * core's DSPR (segment 0x6 / 0x5), loads park SP + small-data anchors, then
 * jumps to ulmk_arch_smp_park().
 *
 * Board WDT for this core is handled by ulmk_board_cpu_wdt_disable_self()
 * inside ulmk_arch_smp_park() — no board SFRs in this file.
 */

    .extern _small_data_
    .extern _small_data2_
    .extern _small_data3_
    .extern _small_data4_
    .extern ulmk_arch_smp_park

/*
 * N: cpu index suffix (1 / 2)
 * SEG: CSA link segment field (6 = DSPR1 @ 0x6xxx, 5 = DSPR2 @ 0x5xxx)
 */
.macro SECONDARY_COLD_ENTRY name, park_top, csa_start, csa_end, seg
    .extern \park_top
    .extern \csa_start
    .extern \csa_end

    .section .text.\name, "ax"
    .global \name
    .type   \name, @function
\name:
    disable

    movh.a  %a10, hi:\park_top
    lea     %a10, [%a10] lo:\park_top

    movh.a  %a2, hi:\csa_start
    lea     %a2, [%a2] lo:\csa_start
    movh.a  %a3, hi:\csa_end
    lea     %a3, [%a3] lo:\csa_end

    lea     %a4, [%a2] 0

.L_\name\()_link:
    lea     %a5, [%a4] 64
    mov.d   %d0, %a5
    extr.u  %d0, %d0, 6, 16
    movh    %d1, \seg
    or      %d0, %d1
    mov.d   %d2, %a5
    mov.d   %d3, %a3
    jge     %d2, %d3, .L_\name\()_last
    st.w    [%a4], %d0
    lea     %a4, [%a5] 0
    j       .L_\name\()_link

.L_\name\()_last:
    mov     %d0, 0
    st.w    [%a4], %d0

    mov.d   %d0, %a2
    extr.u  %d0, %d0, 6, 16
    movh    %d1, \seg
    or      %d0, %d1
    dsync
    mtcr    0xFE38, %d0          /* FCX */
    isync

    lea     %a6, [%a4] 0
    mov.d   %d0, %a6
    extr.u  %d0, %d0, 6, 16
    movh    %d1, \seg
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

    .size   \name, . - \name
.endm

    SECONDARY_COLD_ENTRY _ulmk_cpu1_start, \
	_ulmk_park_stack_cpu1_top, \
	_ulmk_csa_pool_cpu1_start, \
	_ulmk_csa_pool_cpu1_end, 6

    SECONDARY_COLD_ENTRY _ulmk_cpu2_start, \
	_ulmk_park_stack_cpu2_top, \
	_ulmk_csa_pool_cpu2_start, \
	_ulmk_csa_pool_cpu2_end, 5
