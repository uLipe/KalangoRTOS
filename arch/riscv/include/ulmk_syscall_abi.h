/* SPDX-License-Identifier: MIT */
/*
 * Syscall ABI — RISC-V RV32
 * arch/riscv/include/ulmk_syscall_abi.h
 *
 * ecall convention:
 *   a7 = syscall number
 *   a0–a3 = arguments
 *   a0 = return value
 */

#ifndef ULMK_SYSCALL_ABI_RISCV_H
#define ULMK_SYSCALL_ABI_RISCV_H

#include <stdint.h>

#define ULMK_SYSCALL_0(nr, ret) \
	do { \
		register uint32_t _a0 __asm__("a0"); \
		register uint32_t _a7 __asm__("a7") = (uint32_t)(nr); \
		__asm__ volatile("ecall" \
			: "=r"(_a0) \
			: "r"(_a7) \
			: "memory"); \
		(ret) = _a0; \
	} while (0)

#define ULMK_SYSCALL_1(nr, a0v, ret) \
	do { \
		register uint32_t _a0 __asm__("a0") = (uint32_t)(uintptr_t)(a0v); \
		register uint32_t _a7 __asm__("a7") = (uint32_t)(nr); \
		__asm__ volatile("ecall" \
			: "+r"(_a0) \
			: "r"(_a7) \
			: "memory"); \
		(ret) = _a0; \
	} while (0)

#define ULMK_SYSCALL_2(nr, a0v, a1v, ret) \
	do { \
		register uint32_t _a0 __asm__("a0") = (uint32_t)(uintptr_t)(a0v); \
		register uint32_t _a1 __asm__("a1") = (uint32_t)(uintptr_t)(a1v); \
		register uint32_t _a7 __asm__("a7") = (uint32_t)(nr); \
		__asm__ volatile("ecall" \
			: "+r"(_a0) \
			: "r"(_a1), "r"(_a7) \
			: "memory"); \
		(ret) = _a0; \
	} while (0)

#define ULMK_SYSCALL_3(nr, a0v, a1v, a2v, ret) \
	do { \
		register uint32_t _a0 __asm__("a0") = (uint32_t)(uintptr_t)(a0v); \
		register uint32_t _a1 __asm__("a1") = (uint32_t)(uintptr_t)(a1v); \
		register uint32_t _a2 __asm__("a2") = (uint32_t)(uintptr_t)(a2v); \
		register uint32_t _a7 __asm__("a7") = (uint32_t)(nr); \
		__asm__ volatile("ecall" \
			: "+r"(_a0) \
			: "r"(_a1), "r"(_a2), "r"(_a7) \
			: "memory"); \
		(ret) = _a0; \
	} while (0)

#define ULMK_SYSCALL_4(nr, a0v, a1v, a2v, a3v, ret) \
	do { \
		register uint32_t _a0 __asm__("a0") = (uint32_t)(uintptr_t)(a0v); \
		register uint32_t _a1 __asm__("a1") = (uint32_t)(uintptr_t)(a1v); \
		register uint32_t _a2 __asm__("a2") = (uint32_t)(uintptr_t)(a2v); \
		register uint32_t _a3 __asm__("a3") = (uint32_t)(uintptr_t)(a3v); \
		register uint32_t _a7 __asm__("a7") = (uint32_t)(nr); \
		__asm__ volatile("ecall" \
			: "+r"(_a0) \
			: "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a7) \
			: "memory"); \
		(ret) = _a0; \
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

#endif /* ULMK_SYSCALL_ABI_RISCV_H */
