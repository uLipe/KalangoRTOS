/* SPDX-License-Identifier: MIT */
/*
 * RISC-V RV32 kernel startup — arch/riscv/startup.S
 *
 * Hart 0: interrupts off, stack, ulmk_kern_start().
 * Other harts: private park stack, then ulmk_arch_smp_park().
 */

	.extern _ulmk_kernel_stack_top
	.extern ulmk_kern_start
	.extern ulmk_arch_smp_park
	.extern g_ulmk_secondary_stack

	/* Keep in sync with ULMK_SECONDARY_STACK_SIZE in arch/riscv/smp.c */
	.equ	ULMK_SECONDARY_STACK_SIZE, 4096

	.section .text._start, "ax", @progbits
	.global _start
	.type _start, @function
_start:
	csrci   mstatus, 0x8            /* clear MIE — interrupts off */

	csrr    t0, mhartid
	bnez    t0, .Lpark_secondary

	la      sp, _ulmk_kernel_stack_top
	andi    sp, sp, -16             /* enforce 16-byte stack alignment */

	j       ulmk_kern_start
9:
	j       9b

/*
 * Secondary harts must set SP before calling C — prologue stores use the
 * stack.  Park stacks are in .data (g_ulmk_secondary_stack).
 */
.Lpark_secondary:
	la      t1, g_ulmk_secondary_stack
	/* STACK_SIZE=4096 → hart << 12; keep in sync with smp.c */
	slli    t2, t0, 12
	add     t1, t1, t2
	li      t2, ULMK_SECONDARY_STACK_SIZE
	add     sp, t1, t2
	andi    sp, sp, -16
	j       ulmk_arch_smp_park

	.size _start, . - _start
