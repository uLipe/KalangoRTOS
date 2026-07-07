/* SPDX-License-Identifier: MIT */
/*
 * RISC-V RV32 kernel startup — arch/riscv/startup.S
 *
 * Minimal CPU-mandatory prologue only; the shared C runtime bring-up lives in
 * kernel/init/init.c (ulmk_kern_start).  Sequence:
 *   0. Clear mstatus.MIE (interrupts off; kernel unlocks them later)
 *   1. Set sp = _ulmk_kernel_stack_top (16-byte aligned per ABI)
 *   2. Jump to ulmk_kern_start() — does not return
 */

	.extern _ulmk_kernel_stack_top
	.extern ulmk_kern_start

	.section .text._start, "ax", @progbits
	.global _start
	.type _start, @function
_start:
	csrci   mstatus, 0x8            /* clear MIE — interrupts off */

	la      sp, _ulmk_kernel_stack_top
	andi    sp, sp, -16             /* enforce 16-byte stack alignment */

	j       ulmk_kern_start
9:
	j       9b

	.size _start, . - _start
