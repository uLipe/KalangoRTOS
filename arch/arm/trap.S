/* SPDX-License-Identifier: MIT */
/*
 * ARM Cortex-M exception entry — arch/arm/trap.S
 *
 * The hardware stacks the caller-saved core registers ({r0-r3,r12,lr,pc,xpsr})
 * on the interrupted thread's PSP and enters Handler mode on MSP (== the
 * current thread's private kernel stack, per ctx_switch.S).  Each stub simply
 * passes the PSP frame to a C dispatcher and returns to Thread mode.
 *
 * The dispatchers may perform a coroutine context switch (blocking syscall or
 * SysTick preemption), which clobbers LR (the EXC_RETURN token).  Because
 * automatic FP stacking is disabled and threads always run in Thread mode on
 * PSP, the return token is the constant 0xFFFFFFFD; it is reloaded on exit
 * rather than trusted in LR.  PendSV is intentionally never used.
 */

#include <arch_config.h>

	.syntax unified
	.thumb

	.extern _arm_svc_dispatch
	.extern _arm_systick_dispatch
	.extern _arm_irq_dispatch
	.extern _arm_fault_dispatch
	.extern _arm_launch_fresh_handler
	.extern g_arm_first_launch

	.equ EXC_RETURN_THREAD_PSP, 0xFFFFFFFD

	.section .text.trap, "ax", %progbits

/*
 * SVCall.  The caller's exception frame is on PSP for user threads
 * (EXC_RETURN bit2 = 1) but on MSP for the first-launch SVC raised from
 * privileged Thread mode (bit2 = 0).  Pick the right stack, then dispatch —
 * except for the reserved launch immediate, which hands off to the fresh-thread
 * launcher (Handler mode) instead of running a syscall.
 */
	.global SVC_Handler
	.thumb_func
	.type SVC_Handler, %function
SVC_Handler:
	tst	lr, #4
	ite	eq
	mrseq	r0, msp
	mrsne	r0, psp

	ldr	r1, [r0, #24]		/* stacked return PC */
	ldrb	r1, [r1, #-2]		/* SVC immediate     */
	cmp	r1, #ULMK_ARCH_SVC_LAUNCH
	beq	.Lsvc_launch

	bl	_arm_svc_dispatch	/* r0 = frame */
	ldr	lr, =EXC_RETURN_THREAD_PSP
	bx	lr

.Lsvc_launch:
	ldr	r1, =g_arm_first_launch
	ldr	r1, [r1]
	b	_arm_launch_fresh_handler
	.size SVC_Handler, . - SVC_Handler

	.global SysTick_Handler
	.thumb_func
	.type SysTick_Handler, %function
SysTick_Handler:
	bl	_arm_systick_dispatch
	ldr	lr, =EXC_RETURN_THREAD_PSP
	bx	lr
	.size SysTick_Handler, . - SysTick_Handler

/*
 * External NVIC interrupt.  IPSR holds the exception number; the NVIC IRQ line
 * is (IPSR - 16).  Shared by every external vector slot.
 */
	.global _arm_irq_entry
	.thumb_func
	.type _arm_irq_entry, %function
_arm_irq_entry:
	mrs	r0, ipsr
	subs	r0, r0, #16
	bl	_arm_irq_dispatch
	ldr	lr, =EXC_RETURN_THREAD_PSP
	bx	lr
	.size _arm_irq_entry, . - _arm_irq_entry

/*
 * Fault / NMI entry.  Pass the exception number (r0), the offending frame (r1)
 * and the EXC_RETURN token (r2) to the C dispatcher.  A fault from an
 * unprivileged thread is recoverable: the dispatcher rewrites the stacked PC to
 * the thread's exit stub and returns here, and we perform a normal exception
 * return so the thread self-terminates through the syscall path (a coroutine
 * switch out of Handler mode would leak the active fault exception in the NVIC).
 * Fatal faults never return — the dispatcher halts in ulmk_kern_trap_panic().
 */
	.global _arm_fault_entry
	.thumb_func
	.type _arm_fault_entry, %function
_arm_fault_entry:
	mrs	r0, ipsr
	mov	r2, lr			/* EXC_RETURN token */
	tst	lr, #4			/* bit2: 0=MSP frame, 1=PSP */
	ite	eq
	mrseq	r1, msp
	mrsne	r1, psp
	bl	_arm_fault_dispatch
	ldr	lr, =EXC_RETURN_THREAD_PSP
	bx	lr
	.size _arm_fault_entry, . - _arm_fault_entry

	.global _arm_nmi_entry
	.thumb_func
	.type _arm_nmi_entry, %function
_arm_nmi_entry:
	mrs	r0, ipsr
	mov	r2, lr
	tst	lr, #4
	ite	eq
	mrseq	r1, msp
	mrsne	r1, psp
	b	_arm_fault_dispatch
	.size _arm_nmi_entry, . - _arm_nmi_entry

	.global PendSV_Handler
	.thumb_func
	.type PendSV_Handler, %function
PendSV_Handler:
	b	.
	.size PendSV_Handler, . - PendSV_Handler
