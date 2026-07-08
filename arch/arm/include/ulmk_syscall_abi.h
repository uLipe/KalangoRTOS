/* SPDX-License-Identifier: MIT */
/*
 * Syscall ABI — ARM Cortex-M (ARMv7-M / ARMv8-M)
 * arch/arm/include/ulmk_syscall_abi.h
 *
 * SVC convention:
 *   svc #<nr>     syscall number encoded in the 8-bit SVC immediate
 *   r0-r3         arguments (AAPCS)
 *   r0            return value
 *
 * On exception entry the hardware stacks {r0-r3, r12, lr, pc, xpsr} on the
 * caller's PSP; the handler reads the arguments there, extracts the SVC
 * immediate from the instruction at the stacked PC, and writes the return
 * value back into the stacked r0.  Only r0 is modified from the caller's point
 * of view — r1-r3 are restored unchanged on exception return, so they need not
 * be listed as clobbers.
 */

#ifndef ULMK_SYSCALL_ABI_ARM_H
#define ULMK_SYSCALL_ABI_ARM_H

#include <stdint.h>

#define ULMK_SYSCALL_0(nr, ret) \
	do { \
		register uint32_t _r0 __asm__("r0"); \
		__asm__ volatile("svc %[n]" \
			: "=r"(_r0) \
			: [n] "i"(nr) \
			: "memory"); \
		(ret) = _r0; \
	} while (0)

#define ULMK_SYSCALL_1(nr, a0v, ret) \
	do { \
		register uint32_t _r0 __asm__("r0") = (uint32_t)(uintptr_t)(a0v); \
		__asm__ volatile("svc %[n]" \
			: "+r"(_r0) \
			: [n] "i"(nr) \
			: "memory"); \
		(ret) = _r0; \
	} while (0)

#define ULMK_SYSCALL_2(nr, a0v, a1v, ret) \
	do { \
		register uint32_t _r0 __asm__("r0") = (uint32_t)(uintptr_t)(a0v); \
		register uint32_t _r1 __asm__("r1") = (uint32_t)(uintptr_t)(a1v); \
		__asm__ volatile("svc %[n]" \
			: "+r"(_r0) \
			: "r"(_r1), [n] "i"(nr) \
			: "memory"); \
		(ret) = _r0; \
	} while (0)

#define ULMK_SYSCALL_3(nr, a0v, a1v, a2v, ret) \
	do { \
		register uint32_t _r0 __asm__("r0") = (uint32_t)(uintptr_t)(a0v); \
		register uint32_t _r1 __asm__("r1") = (uint32_t)(uintptr_t)(a1v); \
		register uint32_t _r2 __asm__("r2") = (uint32_t)(uintptr_t)(a2v); \
		__asm__ volatile("svc %[n]" \
			: "+r"(_r0) \
			: "r"(_r1), "r"(_r2), [n] "i"(nr) \
			: "memory"); \
		(ret) = _r0; \
	} while (0)

#define ULMK_SYSCALL_4(nr, a0v, a1v, a2v, a3v, ret) \
	do { \
		register uint32_t _r0 __asm__("r0") = (uint32_t)(uintptr_t)(a0v); \
		register uint32_t _r1 __asm__("r1") = (uint32_t)(uintptr_t)(a1v); \
		register uint32_t _r2 __asm__("r2") = (uint32_t)(uintptr_t)(a2v); \
		register uint32_t _r3 __asm__("r3") = (uint32_t)(uintptr_t)(a3v); \
		__asm__ volatile("svc %[n]" \
			: "+r"(_r0) \
			: "r"(_r1), "r"(_r2), "r"(_r3), [n] "i"(nr) \
			: "memory"); \
		(ret) = _r0; \
	} while (0)

typedef struct {
	ulmk_msg_t msg;
	ulmk_tid_t sender;
	uint32_t notif_bits;
	int      is_notif;
} ulmk_recv_or_notif_result_t;

typedef struct {
	const ulmk_msg_t *reply;
	ulmk_msg_t       *next;
	ulmk_tid_t       *next_sender;
} ulmk_reply_recv_args_t;

#endif /* ULMK_SYSCALL_ABI_ARM_H */
