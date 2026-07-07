/* SPDX-License-Identifier: MIT */
/*
 * RISC-V context switch — arch/riscv/ctx_switch.S
 */

	.extern ulmk_arch_cpu_halt
	.extern ulmk_user_thread_entry

	.equ CTX_SIZE, 64
	.equ CTX_S0, 4
	.equ CTX_S1, 8

	.equ MSTATUS_MPIE, (1 << 7)
	.equ MSTATUS_MPP_U, 0

	.text
	.global ulmk_arch_ctx_switch
	.type ulmk_arch_ctx_switch, @function
ulmk_arch_ctx_switch:
	addi    sp, sp, -CTX_SIZE
	sw      ra, 0(sp)
	sw      s0, CTX_S0(sp)
	sw      s1, CTX_S1(sp)
	sw      s2, 12(sp)
	sw      s3, 16(sp)
	sw      s4, 20(sp)
	sw      s5, 24(sp)
	sw      s6, 28(sp)
	sw      s7, 32(sp)
	sw      s8, 36(sp)
	sw      s9, 40(sp)
	sw      s10, 44(sp)
	sw      s11, 48(sp)

	beqz    a0, 1f
	sw      sp, 0(a0)
1:
	lw      sp, 0(a1)

	lw      ra, 0(sp)
	lw      s0, CTX_S0(sp)
	lw      s1, CTX_S1(sp)
	lw      s2, 12(sp)
	lw      s3, 16(sp)
	lw      s4, 20(sp)
	lw      s5, 24(sp)
	lw      s6, 28(sp)
	lw      s7, 32(sp)
	lw      s8, 36(sp)
	lw      s9, 40(sp)
	lw      s10, 44(sp)
	lw      s11, 48(sp)
	addi    sp, sp, CTX_SIZE
	ret
	.size ulmk_arch_ctx_switch, . - ulmk_arch_ctx_switch

	.global _ulmk_thread_trampoline_m
	.type _ulmk_thread_trampoline_m, @function
_ulmk_thread_trampoline_m:
	mv      a0, s0                  /* entry — 1st arg to common entry */
	mv      a1, s1                  /* arg   — 2nd arg to common entry */
	tail    ulmk_user_thread_entry  /* jump: common entry never returns */
	.size _ulmk_thread_trampoline_m, . - _ulmk_thread_trampoline_m

	.global _ulmk_thread_trampoline_u
	.type _ulmk_thread_trampoline_u, @function
_ulmk_thread_trampoline_u:
	li      t0, MSTATUS_MPIE | MSTATUS_MPP_U
	csrw    mstatus, t0
	mv      a0, s0                  /* entry — 1st arg to common entry */
	mv      a1, s1                  /* arg   — 2nd arg to common entry */
	la      t0, ulmk_user_thread_entry
	csrw    mepc, t0
	mret
	.size _ulmk_thread_trampoline_u, . - _ulmk_thread_trampoline_u
