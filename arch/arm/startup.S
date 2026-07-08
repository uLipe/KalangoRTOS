/* SPDX-License-Identifier: MIT */
/*
 * ARM Cortex-M kernel startup — arch/arm/startup.S
 *
 * Minimal CPU-mandatory prologue only; the shared C runtime bring-up lives in
 * kernel/init/init.c (ulmk_kern_start).  At reset the hardware has already
 * loaded MSP from vector[0] and PC from vector[1] (= _start).  Sequence:
 *   0. Mask interrupts (kernel unlocks them later)
 *   1. Re-load MSP = _ulmk_kernel_stack_top (defensive)
 *   2. Point VTOR at the kernel vector table
 *   3. Enable the FPU (CPACR CP10/CP11) when ULMK_ARCH_HAVE_FPU
 *   4. Jump to ulmk_kern_start() — does not return
 */

#include <arch_config.h>

	.syntax unified
	.thumb

	.extern _ulmk_kernel_stack_top
	.extern _ulmk_vector_table
	.extern ulmk_kern_start

	.section .text._start, "ax", %progbits
	.global _start
	.thumb_func
	.type _start, %function
_start:
	cpsid   i                       /* interrupts off */

	ldr     r0, =_ulmk_kernel_stack_top
	msr     msp, r0

	/* VTOR = kernel vector table base */
	ldr     r0, =_ulmk_vector_table
	ldr     r1, =ULMK_ARCH_SCB_VTOR
	str     r0, [r1]
	dsb
	isb

#if ULMK_ARCH_HAVE_FPU
	/* CPACR |= (0xF << 20): full access to CP10/CP11 (single-precision FPU) */
	ldr     r0, =ULMK_ARCH_SCB_CPACR
	ldr     r1, [r0]
	orr     r1, r1, #(0xF << 20)
	str     r1, [r0]
	dsb
	isb

	/*
	 * Disable automatic and lazy FP context preservation (FPCCR.ASPEN=0,
	 * LSPEN=0).  ctx_switch.S saves/restores the FP register file by hand,
	 * so EXC_RETURN must always be the plain basic-frame value 0xFFFFFFFD;
	 * leaving ASPEN set makes the hardware build an extended frame that the
	 * fixed EXC_RETURN in the exception handlers mismatches, corrupting
	 * context (fatal on ARMv8-M).
	 */
	ldr     r0, =ULMK_ARCH_FPU_FPCCR
	ldr     r1, [r0]
	ldr     r2, =(ULMK_ARCH_FPCCR_ASPEN | ULMK_ARCH_FPCCR_LSPEN)
	bic     r1, r1, r2
	str     r1, [r0]
	dsb
	isb
#endif

	bl      ulmk_kern_start
9:
	b       9b

	.size _start, . - _start
