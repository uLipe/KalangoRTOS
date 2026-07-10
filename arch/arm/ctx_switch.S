/* SPDX-License-Identifier: MIT */
/*
 * ARM Cortex-M context switch — arch/arm/ctx_switch.S
 *
 * The kernel demands a synchronous coroutine switch: ulmk_arch_ctx_switch()
 * must return to its caller only when the thread is scheduled again.  On
 * Cortex-M exceptions always execute on MSP, so a single shared kernel stack
 * cannot host several suspended coroutines.  Each thread therefore owns a
 * private kernel stack (carved from the top of its thread stack) and the
 * invariant maintained here is: MSP == current-thread kernel SP.
 *
 * ulmk_arch_ctx_switch swaps MSP the way the RISC-V port swaps its single SP:
 * it pushes the callee-saved core regs, PSP, CONTROL, PRIMASK and (when present)
 * the FP register file onto the outgoing thread's kernel stack, stores MSP into
 * from->ksp, then loads to->ksp into MSP and unwinds — resuming the incoming
 * thread's suspended coroutine.  Automatic FP context stacking is disabled
 * (FPCCR.ASPEN=0) so EXC_RETURN is always the plain 0xFFFFFFFD value and the FP
 * state travels only through this routine.
 *
 * arch_ctx_t field offsets (keep in sync with arch/arm/include/ulmk_arch.h):
 */

#include <arch_config.h>

	.equ CTX_KSP,        0
	.equ CTX_KSTACK_TOP, 4
	.equ CTX_USER_SP,    8
	.equ CTX_ENTRY,      12
	.equ CTX_ARG,        16
	.equ CTX_CONTROL,    20
	.equ CTX_FRESH,      24

	.syntax unified
	.thumb

	.extern ulmk_user_thread_entry

	.section .text.ctx_switch, "ax", %progbits
	.global ulmk_arch_ctx_switch
	.thumb_func
	.type ulmk_arch_ctx_switch, %function
ulmk_arch_ctx_switch:				/* r0 = from, r1 = to */
	push	{r4-r11, lr}
#if ULMK_ARCH_HAVE_FPU
	vpush	{s16-s31}
	vpush	{s0-s15}
	vmrs	r2, fpscr
	push	{r2}
#endif
	mrs	r2, psp
	mrs	r3, control
	mrs	r12, primask
	push	{r2, r3, r12}

	cbz	r0, 1f
	str	sp, [r0, #CTX_KSP]		/* from->ksp = MSP */
1:
	ldr	r2, [r1, #CTX_FRESH]
	cbnz	r2, _arm_launch_fresh

	ldr	sp, [r1, #CTX_KSP]		/* MSP = to->ksp */
	pop	{r2, r3, r12}
	msr	psp, r2
	msr	control, r3
	msr	primask, r12			/* per-thread IRQ-enable state */
	isb
#if ULMK_ARCH_HAVE_FPU
	pop	{r2}
	vmsr	fpscr, r2
	vpop	{s0-s15}
	vpop	{s16-s31}
#endif
	pop	{r4-r11, pc}
	.size ulmk_arch_ctx_switch, . - ulmk_arch_ctx_switch

/*
 * First run of a fabricated thread.  Enter unprivileged Thread mode by faking
 * an exception-return frame on the user stack whose PC is ulmk_user_thread_entry
 * (which lives in *user* text, executable unprivileged) — never a trampoline in
 * kernel text, which an unprivileged fetch would fault on.  entry/arg are passed
 * as r0/r1 through the frame.
 *
 * Handler-mode launch (a fresh thread scheduled inside an exception) returns via
 * BX EXC_RETURN.  The very first launch runs in privileged Thread mode
 * (ulmk_sched_start) where no exception return is possible, so it raises
 * SVC #ULMK_ARCH_SVC_LAUNCH to reach Handler mode and rejoin the common path via
 * _arm_launch_fresh_handler (see trap.S).
 */
	.thumb_func
	.type _arm_launch_fresh, %function
_arm_launch_fresh:				/* r1 = to */
	mrs	r3, ipsr
	cbz	r3, .Lfirst_launch_svc

	.global _arm_launch_fresh_handler
	.thumb_func
_arm_launch_fresh_handler:			/* r1 = to, Handler mode */
	movs	r2, #0
	str	r2, [r1, #CTX_FRESH]
	ldr	sp, [r1, #CTX_KSTACK_TOP]	/* MSP = fresh kernel stack top */
	ldr	r2, [r1, #CTX_USER_SP]
	ldr	r0, [r1, #CTX_ENTRY]		/* -> frame r0 (1st arg) */
	ldr	r12, [r1, #CTX_ARG]		/* -> frame r1 (2nd arg) */
	ldr	r6, [r1, #CTX_CONTROL]

	subs	r2, r2, #32			/* 8-word basic exception frame */
	str	r0, [r2, #0]			/* r0 = entry */
	str	r12, [r2, #4]			/* r1 = arg   */
	movs	r3, #0
	str	r3, [r2, #8]			/* r2  */
	str	r3, [r2, #12]			/* r3  */
	str	r3, [r2, #16]			/* r12 */
	ldr	r3, =_arm_thread_exit_catch
	str	r3, [r2, #20]			/* lr (keep Thumb bit for BX) */
	ldr	r3, =ulmk_user_thread_entry
	bic	r3, r3, #1			/* EXC_RETURN PC must be halfword-aligned */
	str	r3, [r2, #24]			/* pc  */
	ldr	r3, =0x01000000
	str	r3, [r2, #28]			/* xpsr (Thumb bit) */
	msr	psp, r2
	msr	control, r6
	isb
	/*
	 * A fresh thread bypasses the sched_resched frame that would run
	 * ulmk_arch_cpu_irq_restore, so PRIMASK may still be set from the
	 * switch's critical section.  Re-enable interrupts here, otherwise the
	 * thread runs at boosted execution priority and its first SVC escalates
	 * to a forced HardFault.
	 */
	cpsie	i
	ldr	lr, =0xFFFFFFFD
	bx	lr

.Lfirst_launch_svc:
	ldr	r2, =g_arm_first_launch
	str	r1, [r2]
	dsb
	svc	#ULMK_ARCH_SVC_LAUNCH
0:	b	0b
	.size _arm_launch_fresh, . - _arm_launch_fresh

	.thumb_func
	.type _arm_thread_exit_catch, %function
_arm_thread_exit_catch:
	b	.
	.size _arm_thread_exit_catch, . - _arm_thread_exit_catch
