/* SPDX-License-Identifier: MIT */
/*
 * ARM Cortex-M vector table — arch/arm/vectors.S
 *
 * Placed first in KERNEL_FLASH by arch/arm/linker/vectors.ld.in so the reset
 * address (VTOR default 0) lands on word[0]=MSP, word[1]=_start.  All
 * configurable exceptions and external IRQs are routed to handlers in trap.S;
 * PendSV is intentionally unused (context switch happens in SVC/SysTick/IRQ).
 */

	.syntax unified
	.thumb

	.extern _ulmk_kernel_stack_top
	.extern _start
	.extern _arm_nmi_entry
	.extern _arm_fault_entry
	.extern SVC_Handler
	.extern PendSV_Handler
	.extern SysTick_Handler
	.extern _arm_irq_entry

#define ULMK_ARM_NUM_IRQ	96

	.section .isr_vector, "a", %progbits
	.align 9
	.global _ulmk_vector_table
	.type _ulmk_vector_table, %object
_ulmk_vector_table:
	.word _ulmk_kernel_stack_top	/* 0:  initial MSP           */
	.word _start			/* 1:  Reset                 */
	.word _arm_nmi_entry		/* 2:  NMI                   */
	.word _arm_fault_entry		/* 3:  HardFault             */
	.word _arm_fault_entry		/* 4:  MemManage             */
	.word _arm_fault_entry		/* 5:  BusFault              */
	.word _arm_fault_entry		/* 6:  UsageFault            */
	.word _arm_fault_entry		/* 7:  SecureFault (v8-M)    */
	.word 0				/* 8:  reserved              */
	.word 0				/* 9:  reserved              */
	.word 0				/* 10: reserved              */
	.word SVC_Handler		/* 11: SVCall                */
	.word _arm_fault_entry		/* 12: DebugMonitor          */
	.word 0				/* 13: reserved              */
	.word PendSV_Handler		/* 14: PendSV (unused)       */
	.word SysTick_Handler		/* 15: SysTick               */

	.rept ULMK_ARM_NUM_IRQ
	.word _arm_irq_entry		/* 16+: external NVIC IRQs   */
	.endr

	.size _ulmk_vector_table, . - _ulmk_vector_table
