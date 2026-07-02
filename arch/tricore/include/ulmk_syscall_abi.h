/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Syscall ABI — TriCore TC1.6.1 / TC2xx
 * arch/tricore/include/ulmk_syscall_abi.h
 *
 * Included transitively via <ul/syscall_abi.h> → this file.
 * Must not include <ul/microkernel.h>; types (ulmk_msg_t, ulmk_tid_t, etc.)
 * are already in scope because microkernel.h includes us at its end.
 *
 * TriCore SYSCALL convention (arch_api_spec.md §13.4):
 *   SYSCALL #N  →  trap class 6, TIN = N stored in D15 on kernel entry
 *   Arguments:  D4 (a0), D5 (a1), D6 (a2), D7 (a3)  — 32-bit each
 *   Return:     D2  — written by ulmk_arch_syscall_entry() before RFE
 *
 * SYSCALL saves the upper context (D8–D15, A10–A15, PSW, PCXI).
 * D4–D7 are NOT in the upper context and remain live through the trap.
 */

#ifndef ULMK_SYSCALL_ABI_TRICORE_H
#define ULMK_SYSCALL_ABI_TRICORE_H

#include <stdint.h>

/* =========================================================================
 * Inline-assembly SYSCALL_N() macros
 * ========================================================================= */

#define ULMK_SYSCALL_0(nr, ret) \
	do { \
		register uint32_t _d2 __asm__("d2"); \
		__asm__ volatile( \
			"syscall %1" \
			: "=d"(_d2) \
			: "i"(nr) \
			: "memory"); \
		(ret) = _d2; \
	} while (0)

#define ULMK_SYSCALL_1(nr, a0, ret) \
	do { \
		register uint32_t _d4 __asm__("d4") = (uint32_t)(uintptr_t)(a0); \
		register uint32_t _d2 __asm__("d2"); \
		__asm__ volatile( \
			"syscall %1" \
			: "=d"(_d2) \
			: "i"(nr), "d"(_d4) \
			: "memory"); \
		(ret) = _d2; \
	} while (0)

#define ULMK_SYSCALL_2(nr, a0, a1, ret) \
	do { \
		register uint32_t _d4 __asm__("d4") = (uint32_t)(uintptr_t)(a0); \
		register uint32_t _d5 __asm__("d5") = (uint32_t)(uintptr_t)(a1); \
		register uint32_t _d2 __asm__("d2"); \
		__asm__ volatile( \
			"syscall %1" \
			: "=d"(_d2) \
			: "i"(nr), "d"(_d4), "d"(_d5) \
			: "memory"); \
		(ret) = _d2; \
	} while (0)

#define ULMK_SYSCALL_3(nr, a0, a1, a2, ret) \
	do { \
		register uint32_t _d4 __asm__("d4") = (uint32_t)(uintptr_t)(a0); \
		register uint32_t _d5 __asm__("d5") = (uint32_t)(uintptr_t)(a1); \
		register uint32_t _d6 __asm__("d6") = (uint32_t)(uintptr_t)(a2); \
		register uint32_t _d2 __asm__("d2"); \
		__asm__ volatile( \
			"syscall %1" \
			: "=d"(_d2) \
			: "i"(nr), "d"(_d4), "d"(_d5), "d"(_d6) \
			: "memory"); \
		(ret) = _d2; \
	} while (0)

#define ULMK_SYSCALL_4(nr, a0, a1, a2, a3, ret) \
	do { \
		register uint32_t _d4 __asm__("d4") = (uint32_t)(uintptr_t)(a0); \
		register uint32_t _d5 __asm__("d5") = (uint32_t)(uintptr_t)(a1); \
		register uint32_t _d6 __asm__("d6") = (uint32_t)(uintptr_t)(a2); \
		register uint32_t _d7 __asm__("d7") = (uint32_t)(uintptr_t)(a3); \
		register uint32_t _d2 __asm__("d2"); \
		__asm__ volatile( \
			"syscall %1" \
			: "=d"(_d2) \
			: "i"(nr), "d"(_d4), "d"(_d5), "d"(_d6), "d"(_d7) \
			: "memory"); \
		(ret) = _d2; \
	} while (0)

/* =========================================================================
 * Result structs for syscalls that return more than one value.
 * Passed by pointer in the last argument register (D7) so the kernel can
 * fill multiple outputs without exceeding the 4-register limit.
 * ========================================================================= */

/*
 * ulmk_recv_or_notif_result_t — output of ulmk_ep_recv_or_notif().
 * The kernel fills all fields; the wrapper copies them to the caller's
 * output pointers after the SYSCALL returns.
 */
typedef struct {
	ulmk_msg_t msg;
	ulmk_tid_t sender;
	uint32_t notif_bits;
	int      is_notif;	/* 1 = triggered by notification, 0 = IPC */
} ulmk_recv_or_notif_result_t;

/*
 * ulmk_reply_recv_args_t — input/output bundle for ulmk_ep_reply_recv().
 * Carries (reply*, next*, next_sender*) without a 5th register.
 */
typedef struct {
	const ulmk_msg_t *reply;
	ulmk_msg_t       *next;
	ulmk_tid_t       *next_sender;
} ulmk_reply_recv_args_t;

#endif /* ULMK_SYSCALL_ABI_TRICORE_H */
